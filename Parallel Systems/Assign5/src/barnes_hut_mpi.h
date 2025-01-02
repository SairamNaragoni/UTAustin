#pragma once 

#include <mpi.h>
#include "barnes_hut_sequential.h"
#include "visualization.h"
#include "body.h"
#include <iostream>
#include <vector>

void barnes_hut_mpi(struct options_t *opts);

