
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>

sem_t semaphore;

void *thread_function(void *arg) {
    int id = *(int *)arg;
    sem_wait(&semaphore); 

    printf("Thread %d is accessing the resource\n", id);
    sleep(1);
    printf("Thread %d is releasing the resource\n", id);

    sem_post(&semaphore); 
    return NULL;
}

int main() {
    pthread_t threads[5];
    int thread_ids[5];

    sem_init(&semaphore, 0, 2); 

    for (int i = 0; i < 5; i++) {
        thread_ids[i] = i + 1;
        pthread_create(&threads[i], NULL, thread_function, &thread_ids[i]);
    }

    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&semaphore);
    return 0;
}
