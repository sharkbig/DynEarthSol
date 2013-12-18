#include <iostream>

#ifdef USE_OMP
#include <omp.h>
#endif

#include "constants.hpp"
#include "parameters.hpp"
#include "bc.hpp"
#include "fields.hpp"
#include "geometry.hpp"
#include "ic.hpp"
#include "input.hpp"
#include "matprops.hpp"
#include "mesh.hpp"
#include "output.hpp"
#include "remeshing.hpp"
#include "rheology.hpp"


void init(const Param& param, Variables& var)
{
    create_new_mesh(param, var);
    allocate_variables(param, var);

    compute_volume(*var.coord, *var.connectivity, *var.volume);
    *var.volume_old = *var.volume;
    compute_mass(param, *var.egroups, *var.connectivity, *var.volume, *var.mat,
                 var.max_vbc_val, *var.volume_n, *var.mass, *var.tmass);
    compute_shape_fn(*var.coord, *var.connectivity, *var.volume, *var.egroups,
                     *var.shpdx, *var.shpdy, *var.shpdz);

    apply_vbcs(param, var, *var.vel);
    // temperature should be init'd before stress and strain
    initial_temperature(param, var, *var.temperature);
    initial_stress_state(param, var, *var.stress, *var.strain, var.compensation_pressure);
    initial_weak_zone(param, var, *var.plstrain);
}


void update_mesh(const Param& param, Variables& var)
{
    update_coordinate(var, *var.coord);
    surface_processes(param, var, *var.coord);

    var.volume->swap(*var.volume_old);
    compute_volume(*var.coord, *var.connectivity, *var.volume);
    compute_mass(param, *var.egroups, *var.connectivity, *var.volume, *var.mat,
                 var.max_vbc_val, *var.volume_n, *var.mass, *var.tmass);
    compute_shape_fn(*var.coord, *var.connectivity, *var.volume, *var.egroups,
                     *var.shpdx, *var.shpdy, *var.shpdz);
}


int main(int argc, const char* argv[])
{
    double start_time = 0;
#ifdef USE_OMP
    start_time = omp_get_wtime();
#endif

    //
    // read command line
    //
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " config_file\n";
        std::cout << "       " << argv[0] << " -h or --help\n";
        return -1;
    }

    Param param;
    get_input_parameters(argv[1], param);

    //
    // run simulation
    //
    static Variables var; // declared as static to silence valgrind's memory leak detection
    Output output(param, start_time, 0);
    var.time = 0;
    var.steps = 0;

    if (param.control.characteristic_speed == 0)
        var.max_vbc_val = find_max_vbc(param.bc);
    else
        var.max_vbc_val = param.control.characteristic_speed;

    if (! param.sim.is_restarting) {
        init(param, var);
        output.write(var);
    }
    else {
        restart();
    }

    var.dt = compute_dt(param, var);

    int last_regular_frame = 0;  // excluding frames due to output_during_remeshing
    do {
        var.steps ++;
        var.time += var.dt;

        update_temperature(param, var, *var.temperature, *var.ntmp);
        update_strain_rate(var, *var.strain_rate);
        compute_dvoldt(var, *var.ntmp);
        compute_edvoldt(var, *var.ntmp, *var.edvoldt);
        update_stress(var, *var.stress, *var.strain, *var.plstrain, *var.strain_rate);
        update_force(param, var, *var.force);
        // elastic stress/strain are objective (frame-indifferent)
        if (var.mat->rheol_type & MatProps::rh_elastic)
            rotate_stress(var, *var.stress, *var.strain);
        update_velocity(var, *var.vel);
        apply_vbcs(param, var, *var.vel);
        update_mesh(param, var);

        // dt computation is expensive, and dt only changes slowly
        // don't have to do it every time step
        if (var.steps % 10 == 0) var.dt = compute_dt(param, var);

        if ( (var.steps == last_regular_frame * param.sim.output_step_interval) ||
             (var.time > last_regular_frame * param.sim.output_time_interval_in_yr * YEAR2SEC) ) {
            output.write(var);
            last_regular_frame ++;
        }

        if (var.steps % param.mesh.quality_check_step_interval == 0) {
            int quality_is_bad, bad_quality_index;
            quality_is_bad = bad_mesh_quality(param, var, bad_quality_index);
            if (quality_is_bad) {

                if (param.sim.output_during_remeshing) {
                    output.write(var);
                }

                remesh(param, var, quality_is_bad);

                if (param.sim.output_during_remeshing) {
                    output.write(var);
                }
            }
        }

    } while (var.steps < param.sim.max_steps && var.time <= param.sim.max_time_in_yr * YEAR2SEC);

    return 0;
}
