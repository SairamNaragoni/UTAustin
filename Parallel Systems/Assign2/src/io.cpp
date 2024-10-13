#include <io.h>

void read_file(struct options_t* args,
               int*              n_vals,
               real**          input_vals) {

  	// Open file
	std::ifstream in;
	in.open(args->in_file);
	// Get num vals
	in >> *n_vals;

	// Alloc input and output arrays
	*input_vals = (real*) malloc(*n_vals * args -> dims * sizeof(real));

	int index;
	// Read input vals
	for (int i = 0; i < (*n_vals) * args->dims; ++i) {
		in >> index;
		in >> (*input_vals)[i];
	}
}

// void write_file(struct options_t*         args,
//                	struct prefix_sum_args_t* opts) {
//   // Open file
// 	std::ofstream out;
// 	out.open(args->out_file, std::ofstream::trunc);

// 	// Write solution to output file
// 	for (int i = 0; i < opts->n_vals; ++i) {
// 		out << opts->output_vals[i] << std::endl;
// 	}

// 	out.flush();
// 	out.close();
	
// 	// Free memory
// 	free(opts->input_vals);
// 	free(opts->output_vals);
// }
