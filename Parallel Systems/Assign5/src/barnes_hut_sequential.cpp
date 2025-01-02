#include "barnes_hut_sequential.h"

using namespace std;

extern bool timer_debug;
extern bool debug;

// Compute force on a Body from a specific quadtree node
void compute_force_recursive(Body& p, QuadTreeNode& node, double theta) {
    if (!node.body && !node.is_internal) {
        return;  // Empty node, no force
    }

    if (node.is_internal) {
        // Calculate the distance between the Body and the center of mass of this node
        Vec2 diff = node.center_of_mass - p.position;

        double distance = diff.magnitude();

        if(distance == 0)
            return;

        double d = distance;

        if(d < rlimit)
            d = rlimit;

        // cout << "updating force internal node.. for index = " << p.index << " with cx, cy = " << node.center_of_mass.x << ", " << node.center_of_mass.y << endl;

        // Check Multipole Acceptance Criterion (MAC)
        if ((node.side_length) / distance < theta) {
            // Approximate this region as a single mass at the center of mass
            double force_mag = (G * p.mass * node.total_mass) / (d * d);
            Vec2 force_vec = diff * (force_mag/distance);
            p.force += force_vec;
            // print_body(p);
        } else {
            // Recursively compute forces from the children
            compute_force_recursive(p, *node.nw, theta);
            compute_force_recursive(p, *node.ne, theta);
            compute_force_recursive(p, *node.sw, theta);
            compute_force_recursive(p, *node.se, theta);
        }
    } else {
        // This is a leaf node with a single Body
        if (node.body && node.body->mass != OUT_OF_BOUND_MASS && node.body->index != p.index) {
            Vec2 diff = node.body->position - p.position;

            double distance = diff.magnitude();

            if(distance == 0)
                return;

            double d = distance;

            if(d < rlimit)
                d = rlimit;
            
            // cout << "updating force external node.. for index = " << p.index << " with cx, cy = " << node.body->position.x << ", " << node.body->position.y << endl;

            double force_mag = (G * p.mass * node.body->mass) / (d * d);
            Vec2 force_vec = diff * (force_mag/distance);
            p.force += force_vec;
            // print_body(p);
        }
    }
}

// Compute forces on Bodies using the quadtree
void compute_forces(int n_bodies, Body* bodies, QuadTreeNode& root, double theta) {
    // Iterate over each Body
    for (int i=0; i<n_bodies; i++) {
        Body& p = bodies[i];
        if (p.mass != OUT_OF_BOUND_MASS) {  // Skip lost particles
            p.reset_force();
            // Compute force recursively by traversing the tree
            compute_force_recursive(p, root, theta);
        }
    }
}

// Sequential Barnes-Hut simulation function
void barnes_hut_sequential(struct options_t *opts) {
    int n_bodies;
    Body *bodies;

    read_file(opts, &n_bodies, &bodies);

    auto start = chrono::high_resolution_clock::now();

    double build_tree_time = 0.0f, compute_forces_time = 0.0f, update_position_velocity_time = 0.0f;

    // Time step loop
    for (int step = 0; step < opts->steps; step++) {
        auto step_start = chrono::high_resolution_clock::now();

        QuadTreeNode root = build_tree(n_bodies, bodies);

        build_tree_time += chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - step_start).count();

        if(debug) {
            print_quadtree(&root);
        }

        if (opts->visualization) {
            bool quit = render_visualization(n_bodies, bodies, &root);

            if (quit) {
                return ;
            }
        }

        step_start = chrono::high_resolution_clock::now();

        // Compute forces for all particles
        compute_forces(n_bodies, bodies, root, opts->theta);

        compute_forces_time += chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - step_start).count();

        step_start = chrono::high_resolution_clock::now();

        // Update particle positions and velocities
        for (int i=0; i<n_bodies; i++) {
            Body& body = bodies[i];
            body.update_position_velocity(opts->delta);
        }
        
        update_position_velocity_time += chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now() - step_start).count();

    }

    auto end = chrono::high_resolution_clock::now();
    auto diff = chrono::duration_cast<chrono::milliseconds>(end - start);

    if(timer_debug) {
        cout << "total_time:" << diff.count() << endl;
        cout << "avg_step_time:" << diff.count()/opts->steps << endl;
        cout << "build_tree_time:" << build_tree_time/opts->steps << endl;
        cout << "compute_forces_time:" << compute_forces_time/opts->steps << endl;
        cout << "update_position_velocity_time:" << update_position_velocity_time/opts->steps << endl;
    }

    int seconds = diff.count() / 1000;

    cout << setw(6) << setfill('0') << seconds << endl;

    write_file(opts, &n_bodies, bodies);
    free(bodies);
    
    return;
}
