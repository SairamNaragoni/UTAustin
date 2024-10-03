#include <spin_barrier.h>

spin_barrier_t *spin_barrier_alloc() {
    return (spin_barrier_t *)malloc(sizeof(spin_barrier_t));
};

void spin_barrier_init(spin_barrier_t *barrier, int n_threads){
    pthread_spin_init(&(barrier->arrival), 0);
    pthread_spin_init(&(barrier->departure), 0);
    barrier->n_threads = n_threads;
    barrier->counter = 0;
    pthread_spin_lock(&(barrier->departure));
};

void spin_barrier_wait(spin_barrier_t *barrier){
    pthread_spin_lock(&(barrier->arrival));   
    if (++barrier->counter < barrier->n_threads) {  
        pthread_spin_unlock(&(barrier->arrival));
    } else {
        pthread_spin_unlock(&(barrier->departure));
    }

    pthread_spin_lock(&(barrier->departure));

    if (--barrier->counter > 0) {  
        pthread_spin_unlock(&(barrier->departure));
    } else {
        pthread_spin_unlock(&(barrier->arrival));
    }

};

void spin_barrier_destroy(spin_barrier_t *barrier){
    pthread_spin_unlock(&(barrier->departure));
    pthread_spin_destroy(&(barrier->arrival));
    pthread_spin_destroy(&(barrier->departure));
};