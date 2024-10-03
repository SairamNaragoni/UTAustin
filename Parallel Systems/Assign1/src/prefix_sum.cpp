#include "prefix_sum.h"
#include "helpers.h"
#include <cmath>
#include <chrono>

using namespace std;

void barrier_wait(prefix_sum_args_t* args) {
  if (args->spin) {
    spin_barrier_wait((spin_barrier_t *)args->barrier);
  } else {
    pthread_barrier_wait((pthread_barrier_t *)args->barrier);
  }
}

void printArrays(const prefix_sum_args_t* args){
    cout << "Input Values: ";
    for (int i = 0; i < args->n_vals; ++i) {
        cout << args->input_vals[i] << " ";
    }
    cout << endl;

    cout << "Output Values: ";
    for (int i = 0; i < args->n_vals; ++i) {
        cout << args->output_vals[i] << " ";
    }
    cout << endl;
}

void printArgs(const prefix_sum_args_t* args) {
    cout << "Number of Threads: " << args->n_threads << endl;
    cout << "Number of Values: " << args->n_vals << endl;
    cout << "Thread ID: " << args->t_id << endl;

    printArrays(args);
    cout << endl;
}

void printth(int stride, int idx, int idx2, int t_id, string sweep_type) {
    cout << "idx: " << idx << ", idx2: "<<idx2;
    cout << ", stride: " << stride << ", thread-id: " << t_id;
    cout << " sweep: " << sweep_type;
    cout << endl;
}

void* compute_prefix_sum(void *a)
{
    bool debug = false;
    prefix_sum_args_t *args = (prefix_sum_args_t *)a;

    int n_threads = args->n_threads;
    int n_vals = args->n_vals;
    int *input = args->input_vals;
    int *output = args->output_vals;
    int thread_id = args->t_id;

    int n_loops = args->n_loops;
    int (*op)(int, int, int) = args->op;

    for (int i=thread_id; i < n_vals; i += n_threads){
        output[i] = input[i];
    }
    
    barrier_wait(args);

    int stride = 1;
    int idx;
    //Up-Sweep Phase
    for (; stride < n_vals; stride *=2 ) {
        idx = -1;
        for (int i = 0; idx< n_vals; i+=n_threads) {
            idx = (thread_id + 1 + i) * stride * 2 - 1;
            if ((idx < n_vals) && (idx-stride) >=0 ) {
                output[idx] = op(output[idx], output[idx-stride], n_loops);
                // if(debug){
                //     printth(stride, idx, idx-stride, thread_id, "upsweep");
                // }
            } 
        }
        // if(debug) {
        //     printArrays(args);
        // }
        barrier_wait(args);
    }

    //Down-Sweep Phase
    for (; stride > 0; stride /= 2) {
        idx = -1;
        for (int i = 0; idx < n_vals; i+=n_threads) {
            idx = (i + thread_id + 1) * stride * 2 - 1;
            if ((idx < n_vals) && (idx+stride < n_vals)) {
                output[idx+stride] = op(output[idx+stride], output[idx], n_loops);
                // if(debug){
                //     printth(stride, idx+stride, idx, thread_id, "downsweep");
                // }
            }
        }
        // if(debug) {
        //     printArrays(args);
        // }
        barrier_wait(args);
    }

    // if(debug) {
    //     printArrays(args);
    // }

    return 0;
}
