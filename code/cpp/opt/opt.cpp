#include "opt.h"
#include "../Hmesh.h"
#include "../conversions.h"
#include "../state.h"
#include <Optiz/NewtonSolver/Common.h>
#include <Optiz/NewtonSolver/Problem.h>
#include <Optiz/Problem.h>
#include <cstddef>
#include <igl/AABB.h>
#include <memory>

namespace opt {

int num_iters = 0;
float orig_close_to_init_weight = 0.0;
float rigid_weight = 10.0, closeness_weight = 0.1, planarity_weight = 10,
      close_to_init_weight = 0.0, smoothness_weight = 0.1,
      boundary_weight = 100.0, shape_2d_weight = 10.0;

igl::AABB<Eigen::MatrixXd, 3> tree;
std::vector<std::pair<Eigen::VectorXd, Eigen::VectorXd>> target_boundaries;

template <typename T, typename Derived>
T point_segment_sqrd_distance(const Eigen::MatrixBase<Derived>& p, const Eigen::VectorXd& v, const Eigen::VectorXd& w) {
    double l2 = (v - w).squaredNorm();
    if (l2 == 0.0) return (p - v.cast<T>()).squaredNorm();
    T t = std::max(T(0.0), std::min(T(1.0), (p - v.cast<T>()).dot((w - v).cast<T>()) / l2));
    Eigen::Matrix<T, Eigen::Dynamic, 1> projection = v.cast<T>() + t * (w - v).cast<T>();
    return (p - projection).squaredNorm();
}

Eigen::MatrixXi F;

std::unique_ptr<Optiz::Problem> prob = nullptr;

struct FakeFactory {
  using Scalar = double;
  Eigen::MatrixXd &var_block(int i) {
    if (i == 0) {
      return state::opt_lifted;
    }
    return state::opt_ground;
  }
};

auto get_rigid_error(int i, auto &x, bool relative = false,
                     bool sqrd_err = true) {
  auto &e = state::lifted.edges[i];
  auto &x1 = x.var_block(0);
  auto &x2 = x.var_block(1);
  int f = e.fi, fvi = e.fvi, fvj = e.next()->fvi;
  // in R3.
  auto v1 = x1.row(e.vi), v2 = x1.row(e.next()->vi);
  // in R2.
  auto v3 = x2.row(state::ground_closed.F[f][fvi]),
       v4 = x2.row(state::ground_closed.F[f][fvj]);
  if (!relative) {
    return Optiz::sqr((v1 - v2).norm() - (v3 - v4).norm());
  } else {
    double l1 =
        0.5 * (Optiz::val((v1 - v2).norm()) + Optiz::val((v3 - v4).norm()));
    if (sqrd_err) {
      return Optiz::sqr((v1 - v2).norm() - (v3 - v4).norm()) / (l1 * l1);
    }
    return abs((v1 - v2).norm() - (v3 - v4).norm()) / l1;
  }
}

auto get_face_rigid_error(int f, auto &x, bool relative = false,
                          bool sqrd_err = true) {
  using T = FACTORY_TYPE(x);
  auto &x1 = x.var_block(0);
  auto &x2 = x.var_block(1);
  T res(0.0);
  for (int i = 0; i < state::lifted.F[f].size(); i++) {
    for (int j = i + 2; j < state::lifted.F[f].size(); j++) {
      if (i == 0 && j == state::lifted.F[f].size() - 1)
        continue; // Already covered - it's a polygon edge.
      auto v1 = x1.row(state::lifted.F[f][i]);
      auto v2 = x1.row(state::lifted.F[f][j]);
      auto g1 = x2.row(state::ground_closed.F[f][i]);
      auto g2 = x2.row(state::ground_closed.F[f][j]);
      if (!relative) {
        res += Optiz::sqr((v1 - v2).norm() - (g1 - g2).norm());
      } else {
        double l1 =
            0.5 * (Optiz::val((v1 - v2).norm()) + Optiz::val((g1 - g2).norm()));
        if (sqrd_err) {
          res += Optiz::sqr((v1 - v2).norm() - (g1 - g2).norm()) / (l1 * l1);
        } else {
          res += abs((v1 - v2).norm() - (g1 - g2).norm()) / (l1);
        }
      }
    }
  }
  return res;
}

auto get_closeness_error(int i, auto &x) {
  int f;
  Eigen::RowVector3d cp;
  Eigen::RowVector3d p = x.var_block(0).row(i);
  tree.squared_distance(state::target_mesh.V, F, p, f, cp);
  Eigen::Vector3d n = state::target_mesh.face(f).normal();
  return std::abs((p - cp).dot(n));
};

Eigen::Vector3d get_plane_normal(const Eigen::MatrixXd &VF) {
  Eigen::MatrixXd centered = VF;
  centered.rowwise() -= centered.colwise().mean();
  Eigen::MatrixXd cov = centered.transpose() * centered;
  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(cov);
  return eig.eigenvectors().col(0).normalized();
}

double get_planarity_error(int i, auto &xx) {
  if (state::lifted.F[i].size() <= 3)
    return 0.0;
  auto &x = xx.var_block(0);
  Eigen::Vector3d n = get_plane_normal(x(state::lifted.F[i], Eigen::all));
  double err(0.0);
  for (int j = 0; j < state::lifted.F[i].size() - 1; j++) {
    auto vec = x.row(state::lifted.F[i][j + 1]) - x.row(state::lifted.F[i][j]);
    err +=
        std::abs(M_PI / 2 - std::acos(n.dot(vec.normalized()))) / (M_PI) * 180;
  }
  return err;
};

void init() {
  // Force re-creation of the optimization problem with correct data
  // (target_boundaries, opt_lifted, opt_ground are all re-initialized here)
  prob = nullptr;
  num_iters = 0;

  F = convert::to_eig_mat(state::target_mesh.F);
  Eigen::MatrixXd &V = state::target_mesh.V;
  tree.init(V, F);
  
  // Helper lambda to ensure a matrix is Nx3, padding with zeros if needed
  auto pad_to_3d = [](const Eigen::MatrixXd &M) -> Eigen::MatrixXd {
    if (M.cols() >= 3) return M;
    Eigen::MatrixXd M3d = Eigen::MatrixXd::Zero(M.rows(), 3);
    M3d.leftCols(M.cols()) = M;
    return M3d;
  };

  if (state::is_2d_mode) {
      state::opt_lifted = pad_to_3d(state::lifted.V);
      state::opt_lifted.col(2).setZero();
      state::opt_ground = pad_to_3d(state::ground_closed.V);
      state::opt_ground.col(2).setZero();
  } else {
      state::opt_lifted = state::lifted.V;
      state::opt_ground = state::ground_closed.V;
  }

  target_boundaries.clear();
  for (int i = 0; i < state::target_mesh.edges.size(); ++i) {
    if (state::target_mesh.edges[i].is_boundary()) {
      auto e = state::target_mesh.edges[i];
      if (state::is_2d_mode) {
          target_boundaries.push_back({e.origin()->coords().head(2), e.next()->origin()->coords().head(2)});
      } else {
          target_boundaries.push_back({e.origin()->coords(), e.next()->origin()->coords()});
      }
    }
  }
}

void init_prob() {
  prob = std::make_unique<Optiz::Problem>(state::opt_lifted, state::opt_ground);
  prob->options().set_line_search_iters(100);
  prob->options().set_iters(1);
  prob->options().set_report_level(Optiz::Problem::Options::NONE);

  // Rigid energy.
  prob->add_element_energy(state::lifted.edges.size(), [&](int i, auto &x) {
    return rigid_weight * get_rigid_error(i, x, true);
  });

  // Closeness energy.
  if (!state::is_2d_mode) {
      prob->add_element_energy(state::lifted.V.rows(), [&](int i, auto &x) {
        using T = FACTORY_TYPE(x);
        int f;
        Eigen::RowVector3d cp;
        Eigen::RowVector3d p = prob->x(0).row(i);
        tree.squared_distance(state::target_mesh.V, F, p, f, cp);
        auto &x1 = x.var_block(0);
        auto xp = x1.row(i);
        Eigen::Vector3d n = state::target_mesh.face(f).normal();
        return closeness_weight * Optiz::sqr((xp - cp).dot(n));
      });
  }
  
  // Close to the init.
  prob->add_element_energy(state::lifted.V.rows(), [&](int i, auto &x) {
    auto orig = state::lifted.V.row(i);
    auto &x1 = x.var_block(0);
    auto xp = x1.row(i);
    return close_to_init_weight * (xp - orig).squaredNorm();
  });

  // Boundary Force
  if (!target_boundaries.empty()) {
      prob->add_element_energy(state::lifted.V.rows(), [&](int i, auto &x) {
        using T = FACTORY_TYPE(x);
        if (!state::lifted.is_boundary_vertex(i)) return T(0.0);
        auto &x1 = x.var_block(0);

        // dim = 2 in 2D mode, 3 in 3D mode (match boundary segment storage)
        int dim = target_boundaries[0].first.size();

        // Extract value-type position vector of correct dimension
        Eigen::VectorXd xp_val(dim);
        for (int k = 0; k < dim; ++k) xp_val(k) = Optiz::val(x1(i, k));

        double min_dist = std::numeric_limits<double>::max();
        int min_idx = -1;
        for (int j = 0; j < target_boundaries.size(); ++j) {
          double d = point_segment_sqrd_distance<double>(xp_val, target_boundaries[j].first, target_boundaries[j].second);
          if (d < min_dist) {
            min_dist = d;
            min_idx = j;
          }
        }
        if (min_idx == -1) return T(0.0);

        // Autodiff position vector of same dimension
        Eigen::Matrix<T, Eigen::Dynamic, 1> xp_ad(dim);
        for (int k = 0; k < dim; ++k) xp_ad(k) = x1(i, k);
        return boundary_weight * point_segment_sqrd_distance<T>(xp_ad, target_boundaries[min_idx].first, target_boundaries[min_idx].second);
      });
  }


  // 2D Shape valid deployment energy
  // In 2D mode: shape_2d_weight is disabled — it over-constrains the system
  // and prevents the optimizer from freely adapting edge lengths.
  if (!state::is_2d_mode && state::ground_closed.F.size() > 0) {
    prob->add_element_energy(state::ground_closed.F.size(), [&](int f, auto &xx) {
      using T = FACTORY_TYPE(xx);
      auto &x2 = xx.var_block(1);
      T err(0.0);
      auto const& face = state::ground_closed.F[f];
      if (face.size() < 3) return err;
      
      auto p0_orig = state::ground_closed.V.row(face[0]).head(2);
      auto p1_orig = state::ground_closed.V.row(face[1]).head(2);
      Eigen::Vector2d u_orig = p1_orig - p0_orig;
      double L0_sqOrig = u_orig.squaredNorm();
      if (L0_sqOrig < 1e-10) return err;

      for (int j = 2; j < face.size(); j++) {
        auto pj_orig = state::ground_closed.V.row(face[j]).head(2);
        Eigen::Vector2d v_orig = pj_orig - p0_orig;
        
        double alpha = u_orig.dot(v_orig) / L0_sqOrig;
        double beta  = (u_orig.x() * v_orig.y() - u_orig.y() * v_orig.x()) / L0_sqOrig;
        
        Eigen::Matrix<T, 2, 1> p0 = x2.row(face[0]).head(2).transpose();
        Eigen::Matrix<T, 2, 1> p1 = x2.row(face[1]).head(2).transpose();
        Eigen::Matrix<T, 2, 1> pj = x2.row(face[j]).head(2).transpose();
        
        Eigen::Matrix<T, 2, 1> u = p1 - p0;
        Eigen::Matrix<T, 2, 1> v = pj - p0;
        
        Eigen::Matrix<T, 2, 1> u_perp;
        u_perp << -u.y(), u.x();
        
        Eigen::Matrix<T, 2, 1> expected_v = alpha * u + beta * u_perp;
        err += (v - expected_v).squaredNorm();
      }
      return shape_2d_weight * err;
    });
  }

  // Planarity.
  if (!state::is_2d_mode && state::lifted.max_face_degree() > 3) {
    prob->add_element_energy(state::lifted.F.size(), [&](int i, auto &x) {
      using T = FACTORY_TYPE(x);
      if (state::lifted.F[i].size() <= 3)
        return T(0.0);
      Eigen::Vector3d n =
          get_plane_normal(prob->x(0)(state::lifted.F[i], Eigen::all));
      T err(0.0);
      auto &x1 = x.var_block(0);
      for (int j = 0; j < state::lifted.F[i].size() - 1; j++) {
        for (int k = j + 1; k < state::lifted.F[i].size(); k++) {
          Eigen::RowVector3<T> vec =
              x1.row(state::lifted.F[i][k]) - x1.row(state::lifted.F[i][j]);
          double vec_val = Optiz::val(vec.norm());
          err += Optiz::sqr(n.dot(vec) / vec_val);
        }
      }
      return planarity_weight * err;
    });
    prob->add_element_energy(state::lifted.F.size(), [&](int i, auto &x) {
      return rigid_weight * get_face_rigid_error(i, x, true);
    });
  }

  // In 2D mode: pin Z coordinate to 0 strongly
  if (state::is_2d_mode) {
    prob->add_element_energy(state::lifted.V.rows(), [&](int i, auto &x) {
      auto &x1 = x.var_block(0);
      return 100000.0 * Optiz::sqr(x1(i, 2));
    });
    prob->add_element_energy(state::ground_closed.V.rows(), [&](int i, auto &x) {
      auto &x2 = x.var_block(1);
      return 100000.0 * Optiz::sqr(x2(i, 2));
    });
  }
}

void reset_optimization() {
  if (state::is_2d_mode) {
    auto pad_to_3d = [](const Eigen::MatrixXd &M) -> Eigen::MatrixXd {
      if (M.cols() >= 3) return M;
      Eigen::MatrixXd M3d = Eigen::MatrixXd::Zero(M.rows(), 3);
      M3d.leftCols(M.cols()) = M;
      return M3d;
    };
    state::opt_lifted = pad_to_3d(state::lifted.V);
    state::opt_lifted.col(2).setZero();
    state::opt_ground = pad_to_3d(state::ground_closed.V);
    state::opt_ground.col(2).setZero();
  } else {
    state::opt_lifted = state::lifted.V;
    state::opt_ground = state::ground_closed.V;
  }
  close_to_init_weight = orig_close_to_init_weight;
  num_iters = 0;
  prob = nullptr;
  init_prob();
}

void optimize_rigidity() {
  if (F.rows() == 0) {
    init();
  }

  if (!prob) {
    init_prob();
  }
  prob->optimize();
  state::opt_lifted = prob->x(0);
  state::opt_ground = prob->x(1);
  
  // Hard-clamp Z to exactly 0 in 2D mode
  if (state::is_2d_mode) {
    state::opt_lifted.col(2).setZero();
    state::opt_ground.col(2).setZero();
  }

  if (num_iters++ % 20 == 0)
    close_to_init_weight *= 0.8;
}

void get_errors(double *rigid_avg, double *rigid_max, double *close_avg,
                double *close_max, double *planarity_avg,
                double *planarity_max) {
  if (F.rows() == 0) {
    init();
  }
  auto factory = FakeFactory();
  Eigen::VectorXd rigid_errs(state::lifted.edges.size());
  for (int i = 0; i < state::lifted.edges.size(); i++) {
    rigid_errs(i) = (get_rigid_error(i, factory, true, false));
  }
  if (state::lifted.max_face_degree() > 3) {
    rigid_errs.conservativeResize(rigid_errs.size() + state::lifted.F.size());
    for (int i = 0; i < state::lifted.F.size(); i++) {
      rigid_errs(state::lifted.edges.size() + i) =
          (get_face_rigid_error(i, factory, true, false));
    }
  }
  if (rigid_avg)
    *rigid_avg = rigid_errs.mean();
  if (rigid_max)
    *rigid_max = rigid_errs.maxCoeff();

  Eigen::VectorXd close_errs(state::lifted.V.rows());
  for (int i = 0; i < state::lifted.V.rows(); i++) {
    close_errs(i) = get_closeness_error(i, factory);
  }
  if (close_avg)
    *close_avg = close_errs.mean();
  if (close_max)
    *close_max = close_errs.maxCoeff();

  if (planarity_avg && planarity_max) {
    std::vector<double> errs_vec;
    for (int i = 0; i < state::lifted.F.size(); i++) {
      if (state::lifted.F[i].size() == 3)
        continue;
      errs_vec.push_back(get_planarity_error(i, factory));
    }
    if (errs_vec.empty()) {
      *planarity_avg = 0;
      *planarity_max = 0;
      return;
    } else {
      Eigen::Map<Eigen::VectorXd> planarity_errs(errs_vec.data(),
                                                 errs_vec.size());
      *planarity_avg = planarity_errs.mean();
      *planarity_max = planarity_errs.maxCoeff();
    }
  }
}

} // namespace opt