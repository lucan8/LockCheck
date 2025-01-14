#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <dlfcn.h>
#include <unistd.h>
#include <semaphore.h>

pthread_mutex_t mutex;
sem_t sem;
const int nr_threads = 2;
pthread_t threads[2];
int x = 5;

void* runner1(){
    pthread_mutex_lock(&mutex);
    printf("Mutex lock success %ld\n", pthread_self());
    x++;
    pthread_mutex_unlock(&mutex);
    printf("Mutex unlock success %ld\n", pthread_self());

    return NULL;
}

void* runner2(){
    sem_wait(&sem);
    printf("Sem wait success %ld\n", pthread_self());
    x++;
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
}

