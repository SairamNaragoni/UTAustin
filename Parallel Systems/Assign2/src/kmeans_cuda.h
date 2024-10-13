#pragma once

#include <stdio.h>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <chrono>
#include <cuda_runtime.h>

#include "helpers.h"
#include "seed.h"
#include "argparse.h"
#include "from_sdk/helper_cuda.h" //taken from sdk samples provided

int kmeans_cuda(int num_points, real *points, struct options_t *opts, int* cluster_id_of_points, real* centroids, bool use_shared_memory, double *per_iteration_time);