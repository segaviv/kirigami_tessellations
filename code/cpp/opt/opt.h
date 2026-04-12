#pragma once

namespace opt {

extern float rigid_weight, closeness_weight, planarity_weight,
    close_to_init_weight, smoothness_weight, boundary_weight, shape_2d_weight;

void init();
void optimize_rigidity();
void reset_optimization();

void get_errors(double *rigid_avg, double *rigid_max, double *close_avg,
                double *close_max, double *planarity_avg, double *planarity_max);
} // namespace opt