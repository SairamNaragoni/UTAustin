#include <chrono>
#include <cfloat>
#include "argparse.h"
#include "io.h"
#include "seed.h"
#include "kmeans_sequential.h"
#include "kmeans_cuda.h"
#include "kmeans_thrust.h"
#include "helpers.h"

using namespace std;

extern bool timer_debug;

int main(int argc, char **argv) {
  struct options_t opts;
  get_opts(argc, argv, &opts);

  int n_points;
  real *points;
  read_file(&opts, &n_points, &points);

  int *cluster_id_of_points = (int *)malloc(n_points * sizeof(int));

  real *centroids = (real *)malloc(opts.num_clusters * opts.dims * sizeof(real));
  k_means_init_random_centroids(n_points, opts.dims, points, opts.num_clusters, centroids, opts.seed);

  int iterations = 0;
  double per_iteration_time = 0;

  auto start = std::chrono::high_resolution_clock::now();

  switch (opts.algorithm)
  {
    case 0:
      iterations = kmeans_sequential(n_points, points, &opts, cluster_id_of_points, centroids);
      break;
    case 1:
      iterations = kmeans_cuda(n_points, points, &opts, cluster_id_of_points, centroids, false, &per_iteration_time);
      break;
    case 2:
      iterations = kmeans_cuda(n_points, points, &opts, cluster_id_of_points, centroids, true, &per_iteration_time);
      break;
    case 3:
      iterations = kmeans_thrust(n_points, points, &opts, cluster_id_of_points, centroids, &per_iteration_time);
      break;
  }

  auto end = chrono::high_resolution_clock::now();
  auto difference = chrono::duration<double, milli>(end - start);

  if (opts.algorithm == 0) {
    per_iteration_time = difference.count() / iterations;
  }

  if(timer_debug) {
    printf("main_per_iteration: %f ms \n", difference.count() / iterations);
    printf("main_total: %f ms \n", difference.count());
  }

  printf("%d,%lf\n", iterations, per_iteration_time);

  if (opts.show_centroids) {
    print_centroids(centroids, opts.num_clusters, opts.dims);
  } else {
    print_clusters(n_points, cluster_id_of_points);
  }

  free(cluster_id_of_points);
  free(points);
  free(centroids);
}