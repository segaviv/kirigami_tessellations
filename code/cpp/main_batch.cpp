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
#include <fstream>
#include <igl/readOBJ.h>

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
  for (int i = 0; i < 500; ++i) { // Run for 500 iterations
    opt::optimize_rigidity();
  }
  
  double rigid_avg, rigid_max, close_avg, close_max, planarity_avg, planarity_max;
  opt::get_errors(&rigid_avg, &rigid_max, &close_avg, &close_max, &planarity_avg, &planarity_max);
  std::cout << "Optimization Finished. Errors:" << std::endl;
  std::cout << " - rigid_avg: " << rigid_avg << std::endl;
  std::cout << " - close_avg: " << close_avg << std::endl;

  // 3. Export Output
  Eigen::MatrixXd export_opt_lifted(state::opt_lifted.rows(), 3);
  export_opt_lifted.setZero();
  export_opt_lifted.leftCols(state::opt_lifted.cols()) = state::opt_lifted;
  export_opt_lifted.rowwise() -= export_opt_lifted.colwise().mean();

  // Helper: write OBJ with polygonal faces (quads, n-gons)
  auto writePolyOBJ = [](const std::string& path,
                          const Eigen::MatrixXd& V,
                          const std::vector<std::vector<int>>& F) {
    std::ofstream out(path);
    for (int i = 0; i < V.rows(); ++i)
      out << "v " << V(i,0) << " " << V(i,1) << " " << V(i,2) << "\n";
    for (const auto& face : F) {
      out << "f";
      for (int idx : face) out << " " << (idx + 1); // 1-indexed
      out << "\n";
    }
    out.close();
  };

  std::string out_lifted = output_prefix + "_lifted.obj";
  std::string out_ground = output_prefix + "_ground.obj";

  Eigen::MatrixXd export_opt_ground(state::opt_ground.rows(), 3);
  export_opt_ground.setZero();
  export_opt_ground.leftCols(state::opt_ground.cols()) = state::opt_ground;

  writePolyOBJ(out_lifted, export_opt_lifted, state::lifted.F);
  writePolyOBJ(out_ground, export_opt_ground, state::ground_closed.F);

  // Export state::ground: genuinely opened 2D positions (before circle projection)
  if (state::ground.V.rows() > 0) {
    Eigen::MatrixXd V_opened(state::ground.V.rows(), 3);
    V_opened.setZero();
    V_opened.leftCols(state::ground.V.cols()) = state::ground.V;
    writePolyOBJ(output_prefix + "_opened.obj", V_opened, state::ground.F);
    std::cout << "Saved opened 2D pattern to " << output_prefix << "_opened.obj" << std::endl;
  }

  std::cout << "Saved deployed (opt) to " << out_lifted << std::endl;
  std::cout << "Saved folded (opt) to "   << out_ground  << std::endl;

  return 0;
}


