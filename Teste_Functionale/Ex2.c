#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
 
sem_t sem;
int buffer = 0;
 
void *producer(void *arg) {
    for (int i = 0; i < 5; i++) {
        sem_wait(&sem);
        buffer++;
        printf("Producer produced: %d\n", buffer);
        sem_post(&sem);
        sleep(1);
    }
    return NULL;
}
 
void *consumer(void *arg) {
    for (int i = 0; i < 5; i++) {
        sem_wait(&sem);
        printf("Consumer consumed: %d\n", buffer);
        buffer--;
        sem_post(&sem);
        sleep(1);
    }
    return NULL;
}
 
int main() {
    pthread_t t1, t2;
 
    sem_init(&sem, 0, 1);
 
    pthread_create(&t1, NULL, producer, NULL);
    pthread_create(&t2, NULL, consumer, NULL);
 
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
 
    sem_destroy(&sem);
 
    return 0;
}
