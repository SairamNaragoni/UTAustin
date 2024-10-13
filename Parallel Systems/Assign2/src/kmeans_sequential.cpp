#include "kmeans_sequential.h"

using namespace std;

extern bool debug;

void assign_points_to_clusters(int num_clusters, int dims, int num_points, real* points, int* cluster_id_of_points, real *centroids){
    for (int i = 0; i < num_points; i++) {
        int best_centroid = -1;
        real best_distance = DBL_MAX;

        for (int c = 0; c < num_clusters; c++) {
            real distance = 0.0;

            for (int d = 0; d < dims; d++) {
                real diff = points[i * dims + d] - centroids[c * dims + d];
                distance += diff * diff;
            }
            if (distance < best_distance) {
                best_distance = distance;
                best_centroid = c;
            }
        }
        cluster_id_of_points[i] = best_centroid;
    }
}

void update_centroids(int num_clusters, int dims, int num_points, real* points, int* cluster_id_of_points, real *centroids){
    memset(centroids, 0, num_clusters * dims * sizeof(real));

    int *cluster_sizes = (int*)calloc(num_clusters, sizeof(int));

    for (int i = 0; i < num_points; i++) {
        int cluster_id = cluster_id_of_points[i];
        cluster_sizes[cluster_id]++;
        for (int j = 0; j < dims; j++) {
            centroids[cluster_id * dims + j] += points[i * dims + j];
        }
    }

    // Calculate new centroids by averaging the points in each cluster
    for (int c = 0; c < num_clusters; c++) {
        if (cluster_sizes[c] > 0) {
            for (int d = 0; d < dims; d++) {
                centroids[c * dims + d] /= cluster_sizes[c];
            }
        }
    }
    free(cluster_sizes);
}

bool converged(int num_clusters, int dims, real threshold, real *old_centroids, real *new_centroids) {
    for (int c=0; c<num_clusters*dims; c++) {
        real diff = abs(new_centroids[c] - old_centroids[c]);
        if (diff > threshold/dims) {
            return false;
        }
    }
    return true;
}

bool assignment_changed(int num_points, int dims, real threshold, int *old_cluster_id_of_points, int *cluster_id_of_points) {
    for (int c=0; c<num_points; c++) {
        int diff = (old_cluster_id_of_points[c] - cluster_id_of_points[c]);
        if (diff != 0) {
            return false;  // Not converged
        }
    }
    return true;  // All centroids have converged
}

int kmeans_sequential(int num_points, real *points, struct options_t *opts, int *cluster_id_of_points, real *centroids){
    int dims = opts->dims;
    int num_clusters = opts->num_clusters;
    int max_num_iters = opts->max_num_iter;
    real threshold = opts->threshold;
    int *old_cluster_id_of_points;
    real *old_centroids;
    bool use_alternate_convergence = opts->avoid_floating_point_convergence;

    if(use_alternate_convergence)
        old_cluster_id_of_points = (int *)malloc(num_points * sizeof(int));
    else
        old_centroids = (real *)malloc(num_clusters * dims * sizeof(real));

    if(debug){
        cout << "dims = " << dims << endl;
        cout << "num_clusters = " << num_clusters << endl;
        cout << "max_num_iters = " << max_num_iters << endl;
        cout << "threshold = " << threshold << endl;
        cout << "num_points = " << num_points << endl;

        cout << "*********** INITIAL CENTROIDS ***********" << endl;
        print_centroids(centroids, num_clusters, dims);
    }

    bool done = false;
    int iterations = 0;

    while(!done) {
        if(use_alternate_convergence)
            memcpy(old_cluster_id_of_points, cluster_id_of_points, num_points * sizeof(int));
        else
            memcpy(old_centroids, centroids, num_clusters * dims * sizeof(real));

        assign_points_to_clusters(num_clusters, dims, num_points, points, cluster_id_of_points, centroids);

        update_centroids(num_clusters, dims, num_points, points, cluster_id_of_points, centroids);

        iterations++;
        bool is_converged;

        if (use_alternate_convergence)
            is_converged = assignment_changed(num_points, dims, threshold, old_cluster_id_of_points, cluster_id_of_points);
        else
            is_converged = converged(num_clusters, dims, threshold, old_centroids, centroids);

        done = iterations > max_num_iters || is_converged;

        if(debug){
            cout << "*********** CENTROIDS " << iterations << " ***********" << endl;
            print_centroids(centroids, num_clusters, dims);
        }
    }

    if(use_alternate_convergence)
        free(old_cluster_id_of_points);
    else
        free(old_centroids);

    return iterations;

}
