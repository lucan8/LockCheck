#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <semaphore.h>

pthread_mutex_t mutex;
sem_t sem;
const int nr_threads = 7;
pthread_t threads[7];
int x = 5;

void* runner1(){
    pthread_mutex_lock(&mutex);
    printf("Mutex lock success %ld\n", pthread_self());
    for (int i = 0; i <= 100000000; ++i){
        x += rand() % 15;
        x -= 7;
    }
    pthread_mutex_unlock(&mutex);
    printf("Mutex unlock success %ld\n", pthread_self());

    return NULL;
}

void* runner2(){
    sem_wait(&sem);
    printf("Sem wait success %ld\n", pthread_self());
    for (int i = 0; i <= 100000000; ++i){
        x += rand() % 10;
        x -= 5;
    }
    sem_post(&sem);
    printf("Sem post success %ld\n", pthread_self());

    return NULL;
}


int main(){
    if (pthread_mutex_init(&mutex, NULL) !=0){
        perror("Mutex init");
        return -1;
    }
    printf("Mutex Create succes\n");

    if (sem_init(&sem, 0, 1) != 0){
        perror("Sem init");
        return -1;
    }
    printf("Sem Create succes\n");

    for (int i = 0; i < nr_threads; ++i)
        if (i % 2)
            pthread_create(&threads[i], NULL, runner1, NULL);
        else
            pthread_create(&threads[i], NULL, runner2, NULL);
        
    printf("Pthread Create succes\n");

    for (int i = 0;i < nr_threads; ++i)
        pthread_join(threads[i], NULL);
    printf("Pthread Join succes\n");

    pthread_mutex_destroy(&mutex);
    sem_destroy(&sem);
}

