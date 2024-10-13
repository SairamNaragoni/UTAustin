#include "seed.h"

static unsigned long int next = 1;
static unsigned long kmeans_rmax = 32767;

int k_means_rand() {
  next = next * 1103515245 + 12345;
  return (unsigned int)(next/65536) % (kmeans_rmax+1);
}

void k_means_srand(unsigned int seed) {
  next = seed;
}

void k_means_init_random_centroids(int num_points, int dims, real *points, int num_clusters, real *centroids, int seed) {
  k_means_srand(seed);

  for (int i = 0; i < num_clusters; i++) {
    int index = k_means_rand() % num_points;
    std::memcpy(&centroids[i * dims], &points[index * dims], dims * sizeof(real));
  }
}