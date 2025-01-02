#pragma once

#include "helpers.h"
#include "body.h"

int init_visualization(int *argc, char **argv);

void terminate_visualization();

bool render_visualization(int n_bodies,
    struct Body *bodies,
    struct QuadTreeNode *root);
