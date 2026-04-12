#include "Hmesh.h"
#include "conversions.h"
#include "kiri_mesh.h"
#include "multi_grid.h"
#include "parameterization/lift.h"
#include "parameterization/param.h"
#include "opt/opt.h"
#include "state.h"
#include "sym_pattern.h"
#include <iostream>
#include <string>
#include <igl/readOBJ.h>
#include <igl/writeOBJ.h>

void parameterizeMesh(const Eigen::MatrixXd& V, const Eigen::MatrixXi& F) {
  state::target_mesh = utils::Hmesh(V, convert::to_vec_vec(F));
  double scale = std::sqrt(state::target_mesh.area());
  state::target_mesh.V /= scale;
  
  if (state::is_2d_mode) {
      state::UV = state::target_mesh.V.leftCols(2);
  } else {
      Eigen::MatrixXd UV = param::isometric_param(state::target_mesh);
      state::UV = UV;
      state::UV.rowwise() -= state::UV.colwise().mean();
  }
}

void liftPattern() {
  if (state::UV.rows() == 0) {
    std::cout << "No UV coordinates to lift from." << std::endl;
    return;
  }
  geom::KiriMesh max_angle_mesh(state::pattern.mesh());
  max_angle_mesh.generate_cut_mesh();
  max_angle_mesh.generate_rotating_info();
  double max_opening_angle = max_angle_mesh.max_opening_angle();
  state::opening_angle = max_opening_angle;
  double area_ratio = state::target_mesh.area() / max_angle_mesh.mesh.area();
  // Scale to achieve an area_ratio of 5.
  state::scale = std::sqrt(area_ratio / 5);
  utils::Hmesh lifted = param::lift(
      state::target_mesh, state::UV, state::pattern, max_opening_angle,
      state::scale, state::translate.cast<double>(), 0, false);
  opt::reset_optimization();
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    std::cerr << "Usage: " << argv[0] << " <input_mesh.obj> <pattern_name> <output_prefix> [--2d]" << std::endl;
    std::cerr << "Pattern names: waterbomb, arrow, auxetic, hex_triangle" << std::endl;
    return 1;
  }

  std::string input_mesh_path = argv[1];
  std::string pattern_name = argv[2];
  std::string output_prefix = argv[3];

  if (argc > 4 && std::string(argv[4]) == "--2d") {
      state::is_2d_mode = true;
      std::cout << "Starting in 2D strict mode." << std::endl;
  }

  // 1. Read input mesh
  Eigen::MatrixXd V;
  Eigen::MatrixXi F;
  if (!igl::readOBJ(input_mesh_path, V, F)) {
      std::cerr << "Failed to read mesh: " << input_mesh_path << std::endl;
      return 1;
  }
  std::cout << "Loaded mesh with " << V.rows() << " vertices and " << F.rows() << " faces." << std::endl;

  // 2. Select pattern
  if (pattern_name == "waterbomb") {
      state::pattern = geom::SymPattern::water_bomb();
  } else if (pattern_name == "arrow") {
      state::pattern = geom::SymPattern::arrow();
  } else if (pattern_name == "auxetic") {
      state::pattern = geom::SymPattern::triangle_auxetic();
  } else if (pattern_name == "hex_triangle") {
      state::pattern = geom::SymPattern::hexagon_traignle_pattern();
  } else {
      std::cerr << "Unknown pattern name: " << pattern_name << std::endl;
      return 1;
  }

  std::cout << "Parameterizing mesh..." << std::endl;
  parameterizeMesh(V, F);

  std::cout << "Lifting pattern..." << std::endl;
  liftPattern();

  std::cout << "Initializing optimization..." << std::endl;
  opt::init();

  std::cout << "Running optimization..." << std::endl;
  for (int i = 0; i < 200; ++i) { // Run for 200 iterations
    opt::optimize_rigidity();
  }
  
  double rigid_avg, rigid_max, close_avg, close_max, planarity_avg, planarity_max;
  opt::get_errors(&rigid_avg, &rigid_max, &close_avg, &close_max, &planarity_avg, &planarity_max);
  std::cout << "Optimization Finished. Errors:" << std::endl;
  std::cout << " - rigid_avg: " << rigid_avg << std::endl;
  std::cout << " - close_avg: " << close_avg << std::endl;

  // 3. Export Output
  Eigen::MatrixXd centered_opt_lifted = state::opt_lifted;
  centered_opt_lifted.rowwise() -= centered_opt_lifted.colwise().mean();

  // Convert faces format
  Eigen::MatrixXi lifted_F(state::lifted.F.size(), 3);
  for(size_t i=0; i<state::lifted.F.size(); ++i) {
      for(size_t j=0; j<3; ++j) {
          lifted_F(i,j) = state::lifted.F[i][j];
      }
  }
  
  Eigen::MatrixXi floor_F(state::ground_closed.F.size(), 3);
  for(size_t i=0; i<state::ground_closed.F.size(); ++i) {
      if(state::ground_closed.F[i].size() >= 3) {
        for(size_t j=0; j<3; ++j) {
            floor_F(i,j) = state::ground_closed.F[i][j];
        }
      }
  }

  std::string out_lifted = output_prefix + "_lifted.obj";
  std::string out_ground = output_prefix + "_ground.obj";

  igl::writeOBJ(out_lifted, centered_opt_lifted, lifted_F);
  igl::writeOBJ(out_ground, state::opt_ground, floor_F);

  std::cout << "Saved 3D lifting to " << out_lifted << std::endl;
  std::cout << "Saved 2D ground to " << out_ground << std::endl;

  return 0;
}
