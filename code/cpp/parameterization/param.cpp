#include "param.h"
#include "../conversions.h"
#include <Optiz/Optiz.h>
#include <Optiz/Linear/QuadraticObjectiveD.h>
#include <Optiz/Meta/MetaMat.h>
#include "../utils.h"

namespace param {


static Eigen::Matrix2d get_local_frame(const Eigen::MatrixXd &verts,
                                       const Eigen::MatrixXi &faces, int i) {
  int v0 = faces(i, 0), v1 = faces(i, 1), v2 = faces(i, 2);
  const Eigen::Vector3d &e1 = (verts.row(v1) - verts.row(v0)),
                        e2 = verts.row(v2) - verts.row(v1);
  const Eigen::Vector3d &e1_rot = e1.cross(e2).cross(e1).normalized();

  Eigen::Matrix2d local_mat;
  local_mat << e1.norm(), e2.dot(e1) / e1.norm(), 0,
      e2.cross(e1).norm() / e1.norm();
  return local_mat;
}

static Eigen::MatrixXd init_vf(utils::Hmesh &mesh) {
  Optiz::QuadraticObjectiveD obj(std::vector<int>{mesh.nf(), 2});
  obj.add_weighted_equations(
      mesh.edges.size(), [&](int i, auto &x, auto &add_eq) {
        auto &e = mesh.edges[i];
        if (e.is_boundary() || e.vi > e.next()->vi)
          return; // Edge will be covered elsewhere.
        int fj = e.fi, fk = e.twin()->fi;
        Optiz::MetaVec fj_vals(x(fj, 0), x(fj, 1)), fk_vals(x(fk, 0), x(fk, 1));
        Optiz::MetaVec bla =
            (e.frame_rot * Optiz::MetaMat(fj_vals) - Optiz::MetaMat(fk_vals))
                .template col<0>();
        double weight =
            3 * e.vec().norm() / (mesh.face(fj).area() + mesh.face(fk).area());
        add_eq(weight, bla.template get<0>());
        add_eq(weight, bla.template get<1>());
      });
  Eigen::Vector2i known_inds(0, mesh.nf());
  Eigen::Vector2d known_vals(1, 0);
  obj.set_knowns(known_inds, known_vals);
  Eigen::MatrixXd res = obj.solve();
  res.rowwise().normalize();
  return res;
}

static Eigen::MatrixXd init_uv(utils::Hmesh &mesh) {
  auto vf = init_vf(mesh);
  Optiz::QuadraticObjectiveD obj(mesh.nv(), 2);
  auto face_grad = [&](int i) {
    auto v = [&](int j) { return mesh.V.row(mesh.F[i][j]); };
    Eigen::Matrix2d Gtl =
        (utils::matcat<0>(v(0) - v(2), v(1) - v(2)) * mesh.face(i).frame)
            .inverse();
    return utils::matcat<1>(Gtl, -Gtl.rowwise().sum());
  };
  obj.add_weighted_equations_with_rhs(mesh.faces.size(), [&](int i, auto &x,
                                                             auto &add_eq) {
    Eigen::Matrix<double, 2, 3> G = face_grad(i);
    Optiz::MetaVec fj_vals(x(mesh.F[i][0]), x(mesh.F[i][1]), x(mesh.F[i][2]));
    Optiz::MetaMat grad = G * Optiz::MetaMat(fj_vals);
    double weight = mesh.face(i).area();
    add_eq(weight, grad.template col<0>().template get<0>(),
           Eigen::Vector2d(vf(i, 0), -vf(i, 1)));
    add_eq(weight, grad.template col<0>().template get<1>(),
           Eigen::Vector2d(vf(i, 1), vf(i, 0)));
  });
  Eigen::VectorXi known_inds(1);
  known_inds(0) = mesh.F[0][0];
  Eigen::MatrixXd known_vals(1, 2);
  known_vals << 0, 0;
  obj.set_knowns(known_inds, known_vals);
  Eigen::MatrixXd res = obj.solve();
  return res;
}

Eigen::MatrixXd isometric_param(utils::Hmesh& mesh) {
  auto& V = mesh.V;
  auto F = convert::to_eig_mat(mesh.F);
  Eigen::MatrixXd uv = init_uv(mesh);
  // Store the local frame projections.
  std::vector<Eigen::Matrix2d> frame_projections;
  frame_projections.reserve(F.rows());
  for (int i = 0; i < F.rows(); i++) {
    frame_projections.emplace_back(get_local_frame(V, F, i).inverse());
  }

  Optiz::Problem prob(uv);
  prob.options().set_report_level(Optiz::Problem::Options::NONE);
  prob.add_element_energy<6>(F.rows(), [&]<typename T>(int i, auto &x) {
    auto v0 = x.row(F(i, 0)), v1 = x.row(F(i, 1)), v2 = x.row(F(i, 2));
    auto e1 = v1 - v0, e2 = v2 - v1;
    Eigen::Matrix2<T> mat{{e1(0), e2(0)}, {e1(1), e2(1)}};
    Eigen::Matrix2<T> jacobian = mat * frame_projections[i];

    return (jacobian.squaredNorm() + jacobian.inverse().squaredNorm());
  });
  prob.optimize();
  return prob.x();
}
 
} // namespace param