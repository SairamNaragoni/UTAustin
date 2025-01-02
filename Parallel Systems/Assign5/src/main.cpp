#include <chrono>
#include <cfloat>
#include "argparse.h"
#include "io.h"
#include "helpers.h"
#include "barnes_hut_sequential.h"
#include "barnes_hut_mpi.h"
#include "visualization.h"

using namespace std;

extern bool debug;

int main(int argc, char **argv) {
  struct options_t opts;
  get_opts(argc, argv, &opts);

  if (opts.visualization) {
    init_visualization(&argc, argv);
  }

  if(opts.sequential){
    barnes_hut_sequential(&opts);
  } else {
    barnes_hut_mpi(&opts);
  }
}