#pragma once

#include <cstring>
#include <cstdlib>

#include "helpers.h"

int k_means_rand();

void k_means_init_random_centroids(int n_points, int dims, real *points, int num_clusters, real *centroids, int seed);