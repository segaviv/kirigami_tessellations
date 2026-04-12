#pragma once
#include "sym_pattern.h"
#include "Hmesh.h"

namespace state {

extern geom::SymPattern pattern;
extern utils::Hmesh target_mesh, lifted_singularity_mesh;
extern std::string target_mesh_name;
extern std::string pattern_name;
extern Eigen::MatrixXd UV;

extern utils::Hmesh lifted;
extern utils::Hmesh ground;
extern utils::Hmesh ground_closed;
extern Eigen::MatrixXd opt_lifted;
extern Eigen::MatrixXd opt_ground;

extern float scale;
extern float rotate;
extern Eigen::Vector2f translate;
extern float opening_angle;

extern bool is_2d_mode;

extern double rigid_avg_error, rigid_max_error, close_avg_error,
    close_max_error, planarity_avg_error, planarity_max_error, runtime;

struct IterationData {
  Eigen::MatrixXd V_lifted, V_ground;
};
extern std::vector<IterationData> iteration_data;
extern bool record;

} // namespace state