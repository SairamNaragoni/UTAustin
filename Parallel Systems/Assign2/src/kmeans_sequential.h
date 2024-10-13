#pragma once

#include <cstring>
#include <cfloat>
#include <cmath>
#include <cstdlib>

#include "argparse.h"
#include "seed.h"
#include "helpers.h"

int kmeans_sequential(int num_points, real *points, struct options_t *opts, int* cluster_id_of_points, real* centroids);