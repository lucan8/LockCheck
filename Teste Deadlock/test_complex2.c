#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <stdlib.h>
pthread_mutex_t resource1;
sem_t resource2;
int x = 5;

const int nr_threads = 4;
pthread_t threads[4];

void *thread1_func(void *arg) {
    pthread_mutex_lock(&resource1);
    // printf("Thread 1 locked Resource 1\n");
    // sleep(1);
 
    sem_wait(&resource2);
    // printf("Thread 1 locked Resource 2\n");

    for (int i = 0; i <= 100000000; ++i){
        x += rand() % 15;
        x -= 7;
    }

    sem_post(&resource2);
    pthread_mutex_unlock(&resource1);
    return NULL;
}
 
void *thread2_func(void *arg) {
    sem_wait(&resource2);
    //printf("Thread 2 locked Resource 2\n");
 
    pthread_mutex_lock(&resource1);
    //printf("Thread 2 locked Resource 3\n");
    
    for (int i = 0; i <= 100000000; ++i){
        x += rand() % 15;
        x -= 7;
    }

    pthread_mutex_unlock(&resource1);
    sem_post(&resource2);
    return NULL;
}
 

int main() {
    pthread_mutex_init(&resource1, NULL);
    sem_init(&resource2, 0, 1);
 
    for (int i = 0; i < nr_threads; ++i)
        if (i % 2 == 0)
            pthread_create(&threads[i], NULL, thread1_func, NULL);
        else
            pthread_create(&threads[i], NULL, thread2_func, NULL);
       
    
    printf("Pthread Create succes\n");

    for (int i = 0; i < nr_threads; ++i)
        pthread_join(threads[i], NULL);

    printf("Pthread Join succes\n");
 
    pthread_mutex_destroy(&resource1);
    sem_destroy(&resource2);
 
    return 0;
}
 