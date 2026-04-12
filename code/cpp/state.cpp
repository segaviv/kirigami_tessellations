#include "state.h"

namespace state {

utils::Hmesh lifted_singularity_mesh;

geom::SymPattern pattern = geom::SymPattern::arrow();
utils::Hmesh target_mesh;
Eigen::MatrixXd UV;
utils::Hmesh lifted;
utils::Hmesh ground;
utils::Hmesh ground_closed;
float scale = 0.5;
float rotate = 0.0;
Eigen::Vector2f translate = Eigen::Vector2f::Zero();
float opening_angle = 0.0;
std::string target_mesh_name;
std::string pattern_name;
std::vector<IterationData> iteration_data;
bool record = false;
bool is_2d_mode = false;

double rigid_avg_error = 0, rigid_max_error = 0,
              close_avg_error = 0, close_max_error = 0,
              planarity_avg_error = 0, planarity_max_error = 0, runtime = 0;

Eigen::MatrixXd opt_lifted;
Eigen::MatrixXd opt_ground;

} // namespace state