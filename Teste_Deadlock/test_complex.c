#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
 
pthread_mutex_t resource1, resource2, resource3;
// sem_t resource4;
int x = 5;

const int nr_threads = 10;
pthread_t threads[10];

void *thread1_func(void *arg) {
    pthread_mutex_lock(&resource1);
    // printf("Thread 1 locked Resource 1\n");
    // sleep(1);
 
    pthread_mutex_lock(&resource2);
    // printf("Thread 1 locked Resource 2\n");

    for (int i = 0; i <= 100000000; ++i){
        x += rand() % 15;
        x -= 7;
    }

    pthread_mutex_unlock(&resource2);
    pthread_mutex_unlock(&resource1);
    return NULL;
}
 
void *thread2_func(void *arg) {
    pthread_mutex_lock(&resource2);
    //printf("Thread 2 locked Resource 2\n");
 
    pthread_mutex_lock(&resource3);
    //printf("Thread 2 locked Resource 3\n");
    
    for (int i = 0; i <= 100000000; ++i){
        x += rand() % 15;
        x -= 7;
    }

    pthread_mutex_unlock(&resource3);
    pthread_mutex_unlock(&resource2);
    return NULL;
}
 
void *thread3_func(void *arg) {
    pthread_mutex_lock(&resource3);
    //printf("Thread 3 locked Resource 3\n");
    //sleep(1);
 
    pthread_mutex_lock(&resource1);
    //printf("Thread 3 locked Resource 1\n");
    for (int i = 0; i <= 100000000; ++i){
        x += rand() % 15;
        x -= 7;
    }
    pthread_mutex_unlock(&resource1);
    pthread_mutex_unlock(&resource3);
    return NULL;
}
 
int main() {
    pthread_mutex_init(&resource1, NULL);
    pthread_mutex_init(&resource2, NULL);
    pthread_mutex_init(&resource3, NULL);
    // sem_init(&resource4, 0, 1);
 
    for (int i = 0; i < nr_threads; ++i)
        if (i % 3 == 0)
            pthread_create(&threads[i], NULL, thread1_func, NULL);
        else if (i % 3 == 1)
            pthread_create(&threads[i], NULL, thread2_func, NULL);
        else
            pthread_create(&threads[i], NULL, thread3_func, NULL);
    
    printf("Pthread Create succes\n");

    for (int i = 0; i < nr_threads; ++i)
        pthread_join(threads[i], NULL);

    printf("Pthread Join succes\n");
 
    pthread_mutex_destroy(&resource1);
    pthread_mutex_destroy(&resource2);
    pthread_mutex_destroy(&resource3);
    // sem_destroy(&resource4);
 
    return 0;
}
 
