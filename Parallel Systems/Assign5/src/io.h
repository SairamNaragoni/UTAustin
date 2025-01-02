#pragma once

#include <argparse.h>
#include <iostream>
#include <fstream>
#include "helpers.h"
#include "body.h"

void read_file(struct options_t* opts,
               int*              n_bodies,
               Body**             bodies);

void write_file(struct options_t* opts,
                int*              n_bodies,
                Body*             bodies);
