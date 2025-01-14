 
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
 
sem_t semaphore;
 
void *low_priority_worker(void *arg) {
    while (1) {
        sem_wait(&semaphore);
        printf("Low priority worker is executing.\n");
        sleep(2);  
        sem_post(&semaphore);
        usleep(500000);
    }
}
 
void *high_priority_worker(void *arg) {
    while (1) {
        sem_wait(&semaphore);
        printf("High priority worker is executing.\n");
        usleep(100000);  
        sem_post(&semaphore);
    }
}
 
int main() {
    pthread_t t1, t2;
 
    sem_init(&semaphore, 0, 1);  
 
    pthread_create(&t1, NULL, low_priority_worker, NULL);
    pthread_create(&t2, NULL, high_priority_worker, NULL);
 
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
 
    sem_destroy(&semaphore);
 
    return 0;
}
 