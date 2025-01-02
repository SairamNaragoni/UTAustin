#include <io.h>
#include <iomanip>

extern bool debug;

void read_file(struct options_t* opts,
               int*              n_bodies,
               Body**             bodies) {

	std::ifstream infile(opts->in_file);
    if (!infile) {
        std::cerr << "Error opening input file!" << std::endl;
        exit(1);
    }

	infile >> *n_bodies;
    *bodies = new Body[*n_bodies];

    for (int i = 0; i < *n_bodies; ++i) {
        int index;
        double x, y, vx, vy, mass;
        infile >> index >> x >> y >> mass >> vx >> vy;
        (*bodies)[i] = Body(index, x, y, vx, vy, mass);
    }

    infile.close();
}

void write_file(struct options_t* opts,
                int*              n_bodies,
                Body*             bodies) {
    std::ofstream outfile(opts->out_file);
    if (!outfile) {
        std::cerr << "Error opening output file!" << std::endl;
        exit(1);
    }

    outfile << *n_bodies << std::endl;
    for (int i = 0; i < *n_bodies; ++i) {

        if(debug) {
            std::cout << std::fixed << std::setprecision(10) << bodies[i].index << " " 
                    << bodies[i].position.x << " " 
                    << bodies[i].position.y << " "
                    << bodies[i].mass << " "
                    << bodies[i].velocity.x << " "
                    << bodies[i].velocity.y << std::endl;
        }

        outfile << std::fixed << std::setprecision(10) << bodies[i].index << " " 
                << bodies[i].position.x << " " 
                << bodies[i].position.y << " "
                << bodies[i].mass << " "
				<< bodies[i].velocity.x << " "
                << bodies[i].velocity.y << std::endl;
    }
    outfile.close();
}
