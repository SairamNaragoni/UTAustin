#include <argparse.h>

void get_opts(int argc,
              char **argv,
              struct options_t *opts)
{
    if (argc == 1)
    {
        std::cout << "Usage:" << std::endl;
        std::cout << "\t--num_clusters or -k <num_clusters>" << std::endl;
        std::cout << "\t--dims or -d <dims>" << std::endl;
        std::cout << "\t--in or -i <file_path>" << std::endl;
        std::cout << "\t--max_num_iter or -m <max_num_iter>" << std::endl;
        std::cout << "\t--threshold or -t <threshold>" << std::endl;
        std::cout << "\t[Optional flag] --show-centroids or -c" << std::endl;
        std::cout << "\t--seed or -s" << std::endl;
        std::cout << "\t[Optional] --algorithm or -a (defaults to 0 = sequential)" << std::endl;
        std::cout << "\t\t 0 = sequential" << std::endl;
        std::cout << "\t\t 1 = cuda" << std::endl;
        std::cout << "\t\t 2 = cuda_shared_mem" << std::endl;
        std::cout << "\t\t 3 = thrust" << std::endl;
        std::cout << "\t[Optional flag] --avoid_floating_point_convergence or -f (defaults to false)" << std::endl;
        std::cout << "\t[Optional] --threads or -h (defaults to 512)" << std::endl;
        exit(0);
    }

    opts->show_centroids = false;
    opts->algorithm = 0;
    opts->avoid_floating_point_convergence = false;
    opts->threads = 512;

    struct option l_opts[] = {
        {"num_clusters", required_argument, NULL, 'k'},
        {"dims", required_argument, NULL, 'd'},
        {"in", required_argument, NULL, 'i'},
        {"max_num_iter", required_argument, NULL, 'm'},
        {"threshold", required_argument, NULL, 't'},
        {"show-centroids", no_argument, NULL, 'c'},
        {"seed", required_argument, NULL, 's'},
        {"algorithm", optional_argument, NULL, 'a'},
        {"floating_point_convergence", no_argument, NULL, 'f'},
        {"threads", optional_argument, NULL, 'h'},
    };

    int ind, c;
    while ((c = getopt_long(argc, argv, "k:d:i:m:t:cs:a:fh:", l_opts, &ind)) != -1)
    {
        switch (c)
        {
            case 0:
                break;
            case 'k':
                opts->num_clusters = atoi((char *)optarg);
                break;
            case 'd':
                opts->dims = atoi((char *)optarg);
                break;
            case 'i':
                opts->in_file = (char *)optarg;
                break;
            case 'm':
                opts->max_num_iter = atoi((char *)optarg);
                break;
            case 't':
                opts->threshold = atof((char *)optarg);
                break;
            case 'c':
                opts->show_centroids = true;
                break;
            case 's':
                opts->seed = atoi((char *)optarg);
                break;
            case 'a':
                opts->algorithm = atoi((char *)optarg);
                break;
            case 'f':
                opts->avoid_floating_point_convergence = true;
                break;
            case 'h':
                opts->threads = atoi((char *)optarg);
                break;
            case ':':
                std::cerr << argv[0] << ": option -" << (char)optopt << "requires an argument." << std::endl;
                exit(1);
        }
    }
}
