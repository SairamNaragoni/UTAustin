#include <barnes_hut_mpi.h>

using namespace std;

extern bool timer_debug;
extern bool debug;

void barnes_hut_mpi(struct options_t *opts) {
    int rank, size;
    MPI_Init(NULL, NULL);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int n_bodies;
    Body *bodies;

    // Process 0 reads the input file and broadcasts the number of bodies to all processes
    if (rank == 0) {
        read_file(opts, &n_bodies, &bodies);
    }

    auto start = std::chrono::high_resolution_clock::now();
    double start_time = MPI_Wtime();

    double build_tree_time = 0.0f, compute_forces_time = 0.0f, update_position_velocity_time = 0.0f;


    MPI_Bcast(&n_bodies, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Resize the bodies array for all processes
    if (rank != 0) {
        bodies = (Body*) malloc(n_bodies * sizeof(Body));
    }
    MPI_Bcast(bodies, n_bodies * sizeof(Body), MPI_BYTE, 0, MPI_COMM_WORLD);

    // Determine the range of bodies each process will handle
    int part_start = n_bodies * rank / size;
    int part_end = n_bodies * (rank + 1) / size;

    // Time step loop
    for (int step = 0; step < opts->steps; step++) {
        auto step_start = chrono::high_resolution_clock::now();

        // Reset and rebuild the global quadtree based on the full set of bodies
        QuadTreeNode global_root = build_tree(n_bodies, bodies);

        build_tree_time += chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - step_start).count();
        
        if (debug) {
            print_quadtree(&global_root);  // Debug print the global tree
        }

        if (opts->visualization && rank == 0) {
            bool quit = render_visualization(n_bodies, bodies, &global_root);

            if (quit) {
                return ;
            }
        }

        step_start = chrono::high_resolution_clock::now();

        // Compute forces for the subset of bodies each process is responsible for
        for (int i = part_start; i < part_end; i++) {
            compute_forces(1, &bodies[i], global_root, opts->theta);
        }

        compute_forces_time += chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - step_start).count();
        step_start = chrono::high_resolution_clock::now();

        // Update the positions and velocities of the local bodies
        for (int i = part_start; i < part_end; i++) {
            bodies[i].update_position_velocity(opts->delta);
        }

        update_position_velocity_time += chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - step_start).count();

        // Synchronize the updated bodies across all processes
        for (int i = 0; i < size; i++) {
            int pstart = n_bodies * i / size;
            int pend = n_bodies * (i + 1) / size;
            MPI_Bcast(&bodies[pstart], (pend - pstart) * sizeof(Body), MPI_BYTE, i, MPI_COMM_WORLD);
        }
    }

    // Finalize and write output from the master process
    if (rank == 0) {
        write_file(opts, &n_bodies, bodies);
        auto end = chrono::high_resolution_clock::now();
        auto diff = chrono::duration_cast<std::chrono::milliseconds>(end - start);
        if(timer_debug) {
            cout << "total_time:" << diff.count() << endl;
            cout << "avg_step_time:" << diff.count()/opts->steps << endl;
            cout << "build_tree_time:" << build_tree_time/opts->steps << endl;
            cout << "compute_forces_time:" << compute_forces_time/opts->steps << endl;
            cout << "update_position_velocity_time:" << update_position_velocity_time/opts->steps << endl;
        }
        double end_time = MPI_Wtime();
        printf("%f\n", end_time - start_time);
    }

    // Cleanup
    free(bodies);
    MPI_Finalize();

    return;
}



