
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
 
sem_t sem;
 
void *task(void *arg) {
    sem_wait(&sem);
    printf("Thread %ld is executing.\n", (long)arg);
    sleep(1);
    printf("Thread %ld finished.\n", (long)arg);
    sem_post(&sem);
    return NULL;
}
 
int main() {
    pthread_t threads[5];
 
    sem_init(&sem, 0, 2); 
 
    for (long i = 0; i < 5; i++) {
        pthread_create(&threads[i], NULL, task, (void *)i);
    }
 
    for (int i = 0; i < 5; i++) {
        pthread_join(threads[i], NULL);
    }
 
    sem_destroy(&sem);
 
    return 0;
}
 
 
 
 