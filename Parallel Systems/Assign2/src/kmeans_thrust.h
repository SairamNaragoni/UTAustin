#pragma once

#include <thrust/functional.h>
#include <thrust/host_vector.h>
#include <thrust/device_vector.h>
#include <thrust/transform.h>
#include <thrust/reduce.h>
#include <thrust/sequence.h>
#include <thrust/copy.h>
#include <thrust/iterator/zip_iterator.h>
#include <thrust/iterator/discard_iterator.h>
#include <thrust/iterator/counting_iterator.h>
#include <thrust/iterator/transform_iterator.h>
#include <thrust/logical.h>
#include <thrust/transform_reduce.h>
#include <iostream>
#include <cmath>
#include <limits>

#include "argparse.h"
#include "helpers.h"

int kmeans_thrust(int num_points, real *points, struct options_t *opts, int* cluster_id_of_points, real* centroids, double *per_iteration_time);