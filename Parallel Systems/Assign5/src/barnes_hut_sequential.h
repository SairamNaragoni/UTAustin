#pragma once 

#include <cmath>
#include <iostream>
#include <cstring>
#include <stdlib.h>
#include <chrono>
#include <iomanip>

#include "body.h"
#include "helpers.h"
#include "argparse.h"
#include "io.h"
#include "visualization.h"

void compute_forces(int n_bodies, Body* bodies, QuadTreeNode& root, double theta);

void barnes_hut_sequential(struct options_t *opts);