#pragma once

#include <argparse.h>
#include <iostream>
#include <fstream>
#include "helpers.h"

void read_file(struct options_t* args,
               int*              n_vals,
               real**             input_vals);

// void write_file(struct options_t*         args,
//                 struct prefix_sum_args_t* opts);
