#include <pthread.h>
#include <stdio.h>

pthread_mutex_t m1, m2;

void* thread_func(void* arg) {
    pthread_mutex_lock(&m1);
    printf("Thread 1 locked m1\n");
    pthread_mutex_unlock(&m1);

    pthread_mutex_lock(&m2);
    printf("Thread 1 locked m2\n");
    // Missing unlock for m2
    return NULL;
}

int main() {
    pthread_mutex_init(&m1, NULL);
    pthread_mutex_init(&m2, NULL);

    pthread_mutex_lock(&m1);
    pthread_mutex_unlock(&m1);

    return 0;
}
