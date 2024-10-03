#ifndef _SPIN_BARRIER_H
#define _SPIN_BARRIER_H

#include <pthread.h>
#include <iostream>

struct spin_barrier_t {
    pthread_spinlock_t arrival;
    pthread_spinlock_t departure;
    int counter;
    int n_threads;
};

spin_barrier_t *spin_barrier_alloc();

void spin_barrier_init(spin_barrier_t *barrier, int n_threads);

void spin_barrier_wait(spin_barrier_t *barrier);

void spin_barrier_destroy(spin_barrier_t *barrier);

#endif
