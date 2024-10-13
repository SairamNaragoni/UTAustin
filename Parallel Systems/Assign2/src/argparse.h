#pragma once

#include <getopt.h>
#include <stdlib.h>
#include <iostream>

struct options_t {
    int num_clusters;
    int dims;
    char *in_file;
    int max_num_iter;
    double threshold;
    bool show_centroids;
    int seed;
    int algorithm;
    bool avoid_floating_point_convergence;
    int threads;
};

void get_opts(int argc, char **argv, struct options_t *opts);
