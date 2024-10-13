#include <kmeans_cuda.h>

using namespace std;

extern bool debug;
extern bool timer_debug;

__global__ void compute_distances(int num_clusters, int dims, int num_points, real *d_points, real *centroids, real *point_to_centroid_distances) {
    const unsigned int point_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (point_idx < num_points){
        const unsigned int c_idx = threadIdx.y;
        int dim_idx = threadIdx.z;

        real point = d_points[point_idx * dims + dim_idx];
        real centroid = centroids[c_idx * dims + dim_idx];
        real diff = point-centroid;
        diff = diff * diff;
        atomicAdd(&point_to_centroid_distances[point_idx * num_clusters + c_idx], diff);
    }
}

__global__ void compute_distances_shared(int num_clusters, int dims, int num_points, real *d_points, real *centroids, real *point_to_centroid_distances) {
    extern __shared__ real shared_centroids[];

    const unsigned int point_idx = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned int c_idx = threadIdx.y;

    if (c_idx < num_clusters) {
        // Load centroids into shared memory
        for (int dim_idx = 0 ; dim_idx < dims; dim_idx++ ){
            shared_centroids[c_idx * dims + dim_idx] = centroids[c_idx * dims + dim_idx];
        }
    }
    __syncthreads();  // Synchronize to ensure all threads have loaded data
    

    if (point_idx < num_points) {
        real diff = 0.0;
        // Loop over all dimensions to compute the distance
        for (int dim_idx = 0; dim_idx < dims; dim_idx++) {
            real point = d_points[point_idx * dims + dim_idx];
            real centroid = shared_centroids[c_idx * dims + dim_idx];
            diff += (point - centroid) * (point - centroid);
        }
        // Store the computed distance
        point_to_centroid_distances[point_idx * num_clusters + c_idx] = diff;
    }
}

__global__ void assign_points_to_clusters(int num_clusters, int dims, int num_points, real *d_points, real *point_to_centroid_distances, unsigned int* d_cluster_sizes, int *d_cluster_id_of_points) {
    const unsigned int point_idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (point_idx < num_points){
        int best_centroid = -1;
        real best_centroid_dist = INFINITY;

        for (int i = 0; i < num_clusters; i++) {
            real distance = point_to_centroid_distances[point_idx*num_clusters + i];
            if (best_centroid_dist > distance) {
                best_centroid_dist = distance;
                best_centroid = i;
            }
        }
        d_cluster_id_of_points[point_idx] = best_centroid;
        atomicInc(d_cluster_sizes + best_centroid, num_points);
    }
}


__global__ void compute_new_centroids(int dims, int num_points, real *d_points, int *d_cluster_id_of_points, real *centroids) {
    const unsigned int point_idx = blockIdx.x * blockDim.x + threadIdx.x;
    const int dim_idx = threadIdx.y;

    if (point_idx < num_points) {
        const int cluster_idx = d_cluster_id_of_points[point_idx];
        atomicAdd(&centroids[cluster_idx * dims + dim_idx], d_points[point_idx * dims + dim_idx]);
    }    
}

__global__ void compute_new_centroids_shared(int num_clusters, int dims, int num_points, real *d_points, int *d_cluster_id_of_points, real *centroids) {
    extern __shared__ real shared_sum[];

    const unsigned int point_idx = blockIdx.x * blockDim.x + threadIdx.x;

    // Zeroing shared memory for all clusters in the block
    for (int i = threadIdx.x; i < num_clusters * dims; i += blockDim.x) {
        shared_sum[i] = 0.0;
    }
    __syncthreads();

    if (point_idx < num_points) {
        int cluster_idx = d_cluster_id_of_points[point_idx];
        
        // Add point values to shared memory for the correct cluster
        for (int dim_idx = 0; dim_idx < dims; dim_idx++) {
            atomicAdd(&shared_sum[cluster_idx * dims + dim_idx], d_points[point_idx * dims + dim_idx]);
        }
    }
    __syncthreads();

    // Writing accumulated shared memory values back to global memory
    for (int i = threadIdx.x; i < num_clusters * dims; i += blockDim.x) {
        atomicAdd(&centroids[i], shared_sum[i]);
    }
}


__global__ void compute_final_centroids(int dims, int num_clusters, unsigned int* d_cluster_sizes, real *centroids) {
    const unsigned int cluster_idx = blockIdx.x * blockDim.x + threadIdx.x;
    const int dim_idx = threadIdx.y;

    if (cluster_idx < num_clusters) {
        const unsigned int cluster_size = d_cluster_sizes[cluster_idx];
        // Avoid division by zero
        if (cluster_size > 0) {
            centroids[cluster_idx * dims + dim_idx] /= cluster_size;
        }
    }
}

__global__ void converged(int num_clusters, int dims, real threshold, real *old_centroids, real *new_centroids, bool *d_converged) {
    int c = blockIdx.x * blockDim.x + threadIdx.x;

    if (c < num_clusters*dims) {
        real diff = abs(new_centroids[c] - old_centroids[c]);
        if (diff > 10*(threshold/dims)) //x10 as per suggestion in section 3.2.3
            *d_converged = false;
    }
}


//used in alternate implementation
__global__ void assignment_changed(int num_points, int dims, real threshold, int *d_old_cluster_id_of_points, int *cluster_id_of_points, bool *d_converged) {
    int c = blockIdx.x * blockDim.x + threadIdx.x;
    if (c < num_points) {
        int diff = (cluster_id_of_points[c] - d_old_cluster_id_of_points[c]);
        if (diff != 0)
            *d_converged = false;
    }
}


int kmeans_cuda(int num_points, real *h_points, struct options_t *opts, int* cluster_id_of_points, real* h_centroids, bool use_shared_memory, double *per_iteration_time) {
    int num_threads = opts->threads;

    int dims = opts->dims;
    int num_clusters = opts->num_clusters;
    int max_num_iters = opts->max_num_iter;
    real threshold = opts->threshold;
    bool done = false;
    bool is_converged = false;
    int iterations = 0;
    bool use_alternate_convergence = opts->avoid_floating_point_convergence;

    real *d_points;
    real *d_old_centroids;
    real *d_new_centroids;
    int *d_cluster_id_of_points;
    int *d_old_cluster_id_of_points;
    real *d_point_to_centroid_distances;
    unsigned int *d_cluster_sizes;
    bool *d_converged;

    //timers
    cudaEvent_t start, stop, h2d_start, h2d_stop, d2h_start, d2h_stop;

    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventCreate(&h2d_start);
    cudaEventCreate(&h2d_stop);
    cudaEventCreate(&d2h_start);
    cudaEventCreate(&d2h_stop);

    cudaEventRecord(start, 0);

    if(debug){
        cout << "dims = " << dims << endl;
        cout << "num_clusters = " << num_clusters << endl;
        cout << "max_num_iters = " << max_num_iters << endl;
        cout << "threshold = " << threshold << endl;
        cout << "num_points = " << num_points << endl;

        cout << "*********** INITIAL CENTROIDS ***********" << endl;
        print_centroids(h_centroids, num_clusters, dims);
        cout << endl;
    }

    checkCudaErrors(cudaMalloc(&d_points, num_points * dims * sizeof(real)));
    checkCudaErrors(cudaMalloc(&d_old_centroids, num_clusters * dims * sizeof(real)));
    checkCudaErrors(cudaMalloc(&d_new_centroids, num_clusters * dims * sizeof(real)));
    checkCudaErrors(cudaMalloc(&d_cluster_id_of_points, num_points * sizeof(int)));

    if (use_alternate_convergence)
        checkCudaErrors(cudaMalloc(&d_old_cluster_id_of_points, num_points * sizeof(int)));
    else
        checkCudaErrors(cudaMalloc(&d_old_centroids, num_clusters * dims * sizeof(real)));

    checkCudaErrors(cudaMalloc(&d_point_to_centroid_distances, num_points * num_clusters * sizeof(real)));
    checkCudaErrors(cudaMalloc(&d_cluster_sizes, num_clusters * sizeof(int)));
    checkCudaErrors(cudaMalloc(&d_converged, sizeof(bool)));

    cudaEventRecord(h2d_start, 0);
    
    // Copy points and centroids to the device
    checkCudaErrors(cudaMemcpy(d_points, h_points, num_points * dims * sizeof(real), cudaMemcpyHostToDevice));
    checkCudaErrors(cudaMemcpy(d_cluster_id_of_points, cluster_id_of_points, num_points * sizeof(int), cudaMemcpyHostToDevice));
    checkCudaErrors(cudaMemcpy(d_new_centroids, h_centroids, num_clusters * dims * sizeof(real), cudaMemcpyHostToDevice));

    cudaEventRecord(h2d_stop, 0);
    cudaEventSynchronize(h2d_stop);

    float h2d_elapsed_time = 0;
    cudaEventElapsedTime(&h2d_elapsed_time, h2d_start, h2d_stop);

    // Code
    while(!done) {
        if(use_alternate_convergence)
            checkCudaErrors(cudaMemcpy(d_old_cluster_id_of_points, d_cluster_id_of_points, num_points * sizeof(real), cudaMemcpyDeviceToDevice));
        else
            checkCudaErrors(cudaMemcpy(d_old_centroids, d_new_centroids, num_clusters * dims * sizeof(real), cudaMemcpyDeviceToDevice));

        if(use_shared_memory) {
            dim3 block_dim_distances(num_threads/num_clusters, num_clusters); 
            int grid_size_distances = (num_points + block_dim_distances.x - 1) / block_dim_distances.x;
            dim3 grid_dim_distances(grid_size_distances);
            size_t shared_memory_size = block_dim_distances.y * dims * sizeof(real);
            compute_distances_shared<<<grid_dim_distances, block_dim_distances, shared_memory_size >>>(num_clusters, dims, num_points, d_points, d_new_centroids, d_point_to_centroid_distances);
        } else {
            dim3 block_dim_distances(num_threads/num_clusters/dims, num_clusters, dims); 
            int grid_size_distances = (num_points + block_dim_distances.x - 1) / block_dim_distances.x;
            dim3 grid_dim_distances(grid_size_distances);
            compute_distances<<<grid_dim_distances, block_dim_distances >>>(num_clusters, dims, num_points, d_points, d_new_centroids, d_point_to_centroid_distances);
        }

        getLastCudaError("execution of compute_distances() failed\n");

        checkCudaErrors(cudaMemset(d_cluster_sizes, 0, num_clusters * sizeof(int)));
 
        dim3 block_dim_assignment(num_threads);
        int grid_size_assignment = (num_points + block_dim_assignment.x - 1) / block_dim_assignment.x;
        dim3 grid_dim_assignment(grid_size_assignment);
        assign_points_to_clusters<<<grid_dim_assignment, block_dim_assignment>>>(num_clusters, dims, num_points, d_points, d_point_to_centroid_distances, d_cluster_sizes, d_cluster_id_of_points);
        getLastCudaError("execution of assign_points_to_clusters() failed\n");

        checkCudaErrors(cudaMemset(d_new_centroids, 0, num_clusters * dims * sizeof(real)));

        if(use_shared_memory) {
            dim3 block_dim_new_centroids(num_threads);
            int grid_size_new_centroids = (num_points + block_dim_new_centroids.x - 1) / block_dim_new_centroids.x;
            dim3 grid_dim_new_centroids(grid_size_new_centroids);
            compute_new_centroids_shared<<<grid_dim_new_centroids, block_dim_new_centroids, num_clusters * dims * sizeof(real)>>>(num_clusters, dims, num_points, d_points, d_cluster_id_of_points, d_new_centroids);
        } else {
            dim3 block_dim_new_centroids(num_threads/dims, dims);
            int grid_size_new_centroids = (num_points + block_dim_new_centroids.x - 1) / block_dim_new_centroids.x;
            dim3 grid_dim_new_centroids(grid_size_new_centroids);
            compute_new_centroids<<<grid_dim_new_centroids, block_dim_new_centroids>>>(dims, num_points, d_points, d_cluster_id_of_points, d_new_centroids);
        }
        getLastCudaError("execution of compute_new_centroids() failed\n");

        //to reduce floating point error accumulations
        dim3 block_dim_final_centroids(num_threads/dims, dims);
        int grid_size_final_centroids = (num_clusters + block_dim_final_centroids.x - 1) / block_dim_final_centroids.x;
        dim3 grid_dim_final_centroids(grid_size_final_centroids);
        compute_final_centroids<<<grid_dim_final_centroids, block_dim_final_centroids>>>(dims, num_points, d_cluster_sizes, d_new_centroids);
        getLastCudaError("execution of compute_final_centroids() failed\n");

        checkCudaErrors(cudaMemset(d_converged, 1, sizeof(bool)));

        if (use_alternate_convergence) {
            dim3 block_dim_converged(num_threads);
            int grid_size_converged = (num_points + block_dim_converged.x - 1) / block_dim_converged.x;
            dim3 grid_dim_converged(grid_size_converged);
            assignment_changed<<<grid_dim_converged, block_dim_converged>>>(num_points, dims, threshold, d_old_cluster_id_of_points, d_cluster_id_of_points, d_converged);
        } else {
            dim3 block_dim_converged(num_threads);
            int grid_size_converged = (num_clusters * dims + block_dim_converged.x - 1) / block_dim_converged.x;
            dim3 grid_dim_converged(grid_size_converged);
            converged<<<grid_dim_converged, block_dim_converged>>>(num_clusters, dims, threshold, d_old_centroids, d_new_centroids, d_converged);
        }
        getLastCudaError("execution of converged() failed\n");

        checkCudaErrors(cudaMemcpy(&is_converged, d_converged, sizeof(is_converged), cudaMemcpyDeviceToHost));
        iterations++;
        done = (iterations > max_num_iters) || is_converged;

        if(debug){
            cout << "*********** CENTROIDS " << iterations << " ***********" << endl;
            checkCudaErrors(cudaMemcpy(h_centroids, d_new_centroids, num_clusters * dims * sizeof(real), cudaMemcpyDeviceToHost));
            print_centroids(h_centroids, num_clusters, dims);
        }
    }

    cudaEventRecord(d2h_start, 0);

    // Copy results back to host
    checkCudaErrors(cudaMemcpy(h_centroids, d_new_centroids, num_clusters * dims * sizeof(real), cudaMemcpyDeviceToHost));
    checkCudaErrors(cudaMemcpy(cluster_id_of_points, d_cluster_id_of_points, num_points * sizeof(int), cudaMemcpyDeviceToHost));

    cudaEventRecord(d2h_stop, 0);
    cudaEventSynchronize(d2h_stop);

    float d2h_elapsed_time = 0;
    cudaEventElapsedTime(&d2h_elapsed_time, d2h_start, d2h_stop);

    checkCudaErrors(cudaFree(d_points));
    checkCudaErrors(cudaFree(d_new_centroids));
    if(use_alternate_convergence)
        checkCudaErrors(cudaFree(d_old_cluster_id_of_points));
    else
        checkCudaErrors(cudaFree(d_old_centroids));
    checkCudaErrors(cudaFree(d_cluster_id_of_points));
    checkCudaErrors(cudaFree(d_point_to_centroid_distances));
    checkCudaErrors(cudaFree(d_cluster_sizes));
    checkCudaErrors(cudaFree(d_converged));

    cudaEventRecord(stop,0);
    cudaEventSynchronize(stop);

    float elapsed_time = 0;
    cudaEventElapsedTime(&elapsed_time, start, stop);

    *per_iteration_time = elapsed_time/iterations;

    if(timer_debug) {
        float io_time = h2d_elapsed_time + d2h_elapsed_time;
        printf("cuda_per_iteration: %f ms \n", *per_iteration_time);
        printf("cuda_elapsed_time: %f ms \n", elapsed_time);
        printf("cuda_io_time: %f ms \n", io_time);
        printf("cuda_percent_spent_in_io: %f \n", io_time / elapsed_time );
    }

    return iterations;
}