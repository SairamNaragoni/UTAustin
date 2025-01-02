#pragma once

#include <getopt.h>
#include <stdlib.h>
#include <iostream>

struct options_t {
    char *in_file;
    char *out_file;
    int steps;
    double theta;
    double delta;
    bool sequential;
    bool visualization;
};

void get_opts(int argc, char **argv, struct options_t *opts);
