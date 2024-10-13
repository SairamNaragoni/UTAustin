#include "kmeans_thrust.h"

using namespace std;

extern bool debug;
extern bool timer_debug;

template <typename T>
void print_thrust_real(thrust::device_vector<T> vec) {
    cout << "Size=" << vec.size() << endl;
    for (int i=0 ; i < vec.size(); i++){
        cout << vec[i] << " ";
    }
    cout << endl;
}

template <typename T>
struct linear_index_to_row_index : public thrust::unary_function<T,T>
{
  T C; // number of columns
  
  __host__ __device__
  linear_index_to_row_index(T C) : C(C) {}

  __host__ __device__
  T operator()(T i)
  {
    return i / C;
  }
};

struct euclidean_distance_functor : public thrust::unary_function< int, real > {
    int num_clusters;
    int dims;
    real *d_centroids;
    real *d_points;

    euclidean_distance_functor(int _num_clusters, int _dims, real *_d_centroids, real *_d_points) 
        : num_clusters(_num_clusters), dims(_dims), d_centroids(_d_centroids), d_points(_d_points) {}
    
    __device__
    real operator()(int idx) {
        int point_idx = (idx / dims) / num_clusters;
        int cluster_idx = (idx / dims) % num_clusters;
        int dim_idx = idx % dims;
        real diff = d_points[point_idx * dims + dim_idx] - d_centroids[cluster_idx * dims + dim_idx];
        return diff * diff;
    }
};

struct assign_clusters : public thrust::unary_function< int, int > {
    int num_clusters;
    real *d_point_to_centroid_distances;

    assign_clusters(int _num_clusters, real *d_point_to_centroid_distances) 
        : num_clusters(_num_clusters), d_point_to_centroid_distances(d_point_to_centroid_distances) {}
    
    __device__
    real operator()(int idx) {
        real best_distance = INFINITY;
        int best_centroid = -1;
        for (int c=0; c<num_clusters; c++) {
            real distance = d_point_to_centroid_distances[idx * num_clusters + c];
            if (distance < best_distance ){
                best_distance = distance;
                best_centroid = c;
            }
        }
        return best_centroid;
    }
};

struct compute_new_centroids : public thrust::unary_function<void, thrust::tuple<real, int>> {
    int dims;
    int *d_cluster_id_of_points;
    real *centroids;

    compute_new_centroids(int _dims, int *_d_point_cluster_ids, real *_centroids)
        : dims(_dims), d_cluster_id_of_points(_d_point_cluster_ids), centroids(_centroids) {}

    __device__
    void operator()(thrust::tuple<real, int> point_idx_tuple) {
        real point_value = thrust::get<0>(point_idx_tuple);
        int idx = thrust::get<1>(point_idx_tuple);
        int assigned_centroid = d_cluster_id_of_points[idx / dims];
        int centroid_idx = assigned_centroid*dims + idx % dims;
        atomicAdd(&centroids[centroid_idx], point_value);
    }
};

struct compute_cluster_sizes : public thrust::unary_function<void, int> {
    int num_points;
    int *d_cluster_sizes;

    compute_cluster_sizes(int _num_points, int *_d_cluster_sizes)
        : num_points(_num_points), d_cluster_sizes(_d_cluster_sizes) {}

    __device__
    void operator()(int value) {
        atomicAdd(&d_cluster_sizes[value], 1);
    }
};

struct divide_centroids : public thrust::unary_function<real, thrust::tuple<int, int>> {
    int dims;
    int *d_cluster_sizes;

    divide_centroids(int _dims, int *_d_cluster_sizes)
        : dims(_dims), d_cluster_sizes(_d_cluster_sizes) {}

    __device__
    real operator()(thrust::tuple<real, int> centroid_idx_tuple) {
        real summed_centroid_value = thrust::get<0>(centroid_idx_tuple);
        int idx = thrust::get<1>(centroid_idx_tuple);
        int d_cluster_sizes_idx = idx / dims;
        int cluster_size = d_cluster_sizes[d_cluster_sizes_idx];
        if (cluster_size > 0)
            return summed_centroid_value/d_cluster_sizes[d_cluster_sizes_idx];
        else
            return summed_centroid_value;
    }
};

struct converged : public thrust::unary_function<bool, thrust::tuple<real, real>>{
    real threshold;
    int dims;

    converged(real _threshold, int _dims) : threshold(_threshold), dims(_dims) {}

    __device__
    bool operator()(thrust::tuple<real, real> centroid_pair) {
        real new_centroid = thrust::get<0>(centroid_pair);
        real old_centroid = thrust::get<1>(centroid_pair);
        real diff = abs(new_centroid - old_centroid);
        return diff < 10 * (threshold/dims);  // Return true if difference is within threshold, x10 as per suggestion in section 3.2.3
    }
};

struct assignment_changed : public thrust::unary_function<bool, thrust::tuple<int, int>> {

    __device__
    bool operator()(thrust::tuple<int, int> cluster_id_pair) {
        real old_cluster_id = thrust::get<0>(cluster_id_pair);
        real new_cluster_id = thrust::get<1>(cluster_id_pair);
        real diff = (old_cluster_id - new_cluster_id);
        return diff == 0;  // Return true if difference is 0
    }
};


int kmeans_thrust(int num_points, real *points, struct options_t *opts, int* cluster_id_of_points, real* centroids, double *per_iteration_time) {
    int dims = opts->dims;
    int num_clusters = opts->num_clusters;
    int max_num_iters = opts->max_num_iter;
    real threshold = opts->threshold;
    bool done = false;
    int iterations = 0;
    bool use_alternate_convergence = opts->avoid_floating_point_convergence;

    //timers
    cudaEvent_t start, stop, h2d_start, h2d_stop, d2h_start, d2h_stop;

    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventCreate(&h2d_start);
    cudaEventCreate(&h2d_stop);
    cudaEventCreate(&d2h_start);
    cudaEventCreate(&d2h_stop);

    cudaEventRecord(start, 0);

    thrust::device_vector<int> d_cluster_id_of_points(num_points);
    thrust::device_vector<int> d_old_cluster_id_of_points(num_points);
    thrust::device_vector<int> d_cluster_sizes(num_clusters, 0);
    thrust::device_vector<real> d_new_centroids(num_clusters * dims, 0.0f);
    thrust::device_vector<real> d_points(num_points * dims);
    thrust::device_vector<real> d_old_centroids(num_clusters * dims, 0.0f);
    thrust::device_vector<real> d_point_to_centroid_distances(num_points * num_clusters);

    cudaEventRecord(h2d_start, 0);

    // Copy points from host to device
    thrust::copy(points, points + (num_points * dims), d_points.begin());
    // Copy centroids from host to device
    thrust::copy(centroids, centroids + (num_clusters * dims), d_new_centroids.begin());
    // Copy cluster IDs from host to device
    thrust::copy(cluster_id_of_points, cluster_id_of_points + num_points, d_cluster_id_of_points.begin());

    cudaEventRecord(h2d_stop, 0);
    cudaEventSynchronize(h2d_stop);

    float h2d_elapsed_time = 0;
    cudaEventElapsedTime(&h2d_elapsed_time, h2d_start, h2d_stop);

    if(debug){
        cout << "dims = " << dims << endl;
        cout << "num_clusters = " << num_clusters << endl;
        cout << "max_num_iters = " << max_num_iters << endl;
        cout << "threshold = " << threshold << endl;
        cout << "num_points = " << num_points << endl;

        cout << "*********** INITIAL CENTROIDS ***********" << endl;
        // print_centroids(centroids, num_clusters, dims);
        // print_thrust_real(d_new_centroids);
        cout << endl;
    }

    while(!done) {
        if(use_alternate_convergence)
            thrust::copy(d_cluster_id_of_points.begin(), d_cluster_id_of_points.end(), d_old_cluster_id_of_points.begin());
        else
            thrust::copy(d_new_centroids.begin(), d_new_centroids.end(), d_old_centroids.begin());

        euclidean_distance_functor euclidean_distance_functor(
            num_clusters, 
            dims, 
            raw_pointer_cast(d_new_centroids.data()), 
            raw_pointer_cast(d_points.data())
        );

        // Step 1: Compute distances between points and centroids
        thrust::reduce_by_key(
            thrust::make_transform_iterator(thrust::counting_iterator<int>(0), linear_index_to_row_index<int>(dims)),
            thrust::make_transform_iterator(thrust::counting_iterator<int>(num_points * num_clusters * dims), linear_index_to_row_index<int>(dims)),
            thrust::make_transform_iterator(thrust::counting_iterator<int>(0), euclidean_distance_functor),
            thrust::make_discard_iterator(),
            d_point_to_centroid_distances.begin()
        );

        // Step 2: Assign each point to the closest centroid
        assign_clusters assign_clusters(num_clusters, raw_pointer_cast(d_point_to_centroid_distances.data()));
        
        thrust::transform (
            thrust::counting_iterator<int>(0),
            thrust::counting_iterator<int>(num_points),
            d_cluster_id_of_points.begin(),
            assign_clusters
        );

        // Step 3: Update centroids
        thrust::fill(d_new_centroids.begin(), d_new_centroids.end(), 0.0f);

        compute_new_centroids compute_new_centroids(
            dims, 
            raw_pointer_cast(d_cluster_id_of_points.data()), 
            raw_pointer_cast(d_new_centroids.data())
        );

        thrust::for_each(
            thrust::make_zip_iterator(
                thrust::make_tuple(
                    d_points.begin(), 
                    thrust::counting_iterator<int>(0)
                )
            ),
            thrust::make_zip_iterator(
                thrust::make_tuple(
                    d_points.end(), 
                    thrust::counting_iterator<int>(num_points * dims)
                )
            ),
            compute_new_centroids
        );

        thrust::fill(d_cluster_sizes.begin(), d_cluster_sizes.end(), 0);
        thrust::for_each(
            d_cluster_id_of_points.begin(),
            d_cluster_id_of_points.end(),
            compute_cluster_sizes(num_clusters, raw_pointer_cast(d_cluster_sizes.data()))
        );

        thrust::transform(
            thrust::make_zip_iterator(
                thrust::make_tuple(
                    d_new_centroids.begin(), 
                    thrust::counting_iterator<int>(0)
                )
            ),
            thrust::make_zip_iterator(
                thrust::make_tuple(
                    d_new_centroids.end(), 
                    thrust::counting_iterator<int>(num_clusters * dims)
                )
            ),
            d_new_centroids.begin(),
            divide_centroids(dims, raw_pointer_cast(d_cluster_sizes.data()))
        );

        bool is_converged;

        if(use_alternate_convergence){
            is_converged = thrust::transform_reduce(
                thrust::make_zip_iterator(
                    thrust::make_tuple(d_old_cluster_id_of_points.begin(), d_cluster_id_of_points.begin())),
                thrust::make_zip_iterator(
                    thrust::make_tuple(d_old_cluster_id_of_points.end(), d_cluster_id_of_points.end())),
                assignment_changed(),         // Transformation: Compare distance
                true,                     // Initial value for reduction (assume converged)
                thrust::logical_and<bool>()  // Reduction: Return true only if all elements meet the condition
            );
        }
        else{
            is_converged = thrust::transform_reduce(
                thrust::make_zip_iterator(
                    thrust::make_tuple(d_new_centroids.begin(), d_old_centroids.begin())),
                thrust::make_zip_iterator(
                    thrust::make_tuple(d_new_centroids.end(), d_old_centroids.end())),
                converged(threshold, dims),         // Transformation: Compare distance
                true,                     // Initial value for reduction (assume converged)
                thrust::logical_and<bool>()  // Reduction: Return true only if all elements meet the condition
            );
        }

        iterations ++;
        done = (iterations > max_num_iters) || is_converged;
    }

    cudaEventRecord(d2h_start, 0);

    thrust::copy(d_new_centroids.begin(), d_new_centroids.end(), centroids);
    thrust::copy(d_cluster_id_of_points.begin(), d_cluster_id_of_points.end(), cluster_id_of_points);

    cudaEventRecord(d2h_stop, 0);
    cudaEventSynchronize(d2h_stop);

    float d2h_elapsed_time = 0;
    cudaEventElapsedTime(&d2h_elapsed_time, d2h_start, d2h_stop);

    cudaEventRecord(stop,0);
    cudaEventSynchronize(stop);

    float elapsed_time = 0;
    cudaEventElapsedTime(&elapsed_time, start, stop);

    *per_iteration_time = elapsed_time/iterations;

    if(timer_debug) {
        float io_time = h2d_elapsed_time + d2h_elapsed_time;
        printf("thrust_per_iteration: %f ms \n", *per_iteration_time);
        printf("thrust_elapsed_time: %f ms \n", elapsed_time);
        printf("thrust_io_time: %f ms \n", io_time);
        printf("thrust_percent_spent_in_io: %f \n", io_time / elapsed_time );
    }

    return iterations;
}