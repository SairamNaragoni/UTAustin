#include "helpers.h"

bool debug = false; 
bool timer_debug = false; 

void print_clusters(int num_points, int* cluster_id_of_points) {
    printf("clusters:");
    for(int p=0; p < num_points; p++)
        printf(" %d", cluster_id_of_points[p]);
}

void print_centroids(real *centroids, int num_clusters, int dims) {
    for (int clusterId = 0; clusterId < num_clusters; clusterId ++){
        printf("%d ", clusterId);
        for (int d = 0; d < dims; d++)
            printf("%lf ", centroids[clusterId * dims + d]);
        printf("\n");
    }
}

void print_int_array(int *arr) {
    int size = sizeof(arr) / sizeof(arr[0]);
    for (int i = 0; i < size; i++) {
        printf("%i ", arr[i]);
    }
    printf("\n");
}

void print_real_array(real *arr) {
    int size = sizeof(arr) / sizeof(arr[0]);
    for (int i = 0; i < size; i++) {
        printf("%f ", arr[i]);
    }
    printf("\n");
}


template<typename T>
void print_array(T *arr) {
    int size = sizeof(arr) / sizeof(arr[0]);
    for (int i = 0; i < size; i++) {
        printf("%f ", static_cast<double>(arr[i]));
    }
    printf("\n");
}

