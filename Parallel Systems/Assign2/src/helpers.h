#pragma once

#include <stdlib.h>
#include <iostream>
#include <cstring>
#include <cstdio>

using real = float;

void print_clusters(int num_points, int* cluster_id_of_points);

void print_centroids(real *centroids, int num_clusters, int dims);

template<typename T>
void print_array(T *arr);

void print_real_array(real *arr);

void print_int_array(int *arr);