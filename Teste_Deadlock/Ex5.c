 
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>
 
sem_t sem1, sem2;
 
void *thread1_func(void *arg) {
    sem_wait(&sem1);
    printf("Thread 1 acquired Semaphore 1\n");
    sleep(1);
 
    sem_wait(&sem2);
    printf("Thread 1 acquired Semaphore 2\n");
 
    sem_post(&sem2);
    sem_post(&sem1);
    return NULL;
}
 
void *thread2_func(void *arg) {
    sem_wait(&sem2);
    printf("Thread 2 acquired Semaphore 2\n");
    sleep(1);
 
    sem_wait(&sem1);
    printf("Thread 2 acquired Semaphore 1\n");
 
    sem_post(&sem1);
    sem_post(&sem2);
    return NULL;
}
 
int main() {
    pthread_t t1, t2;
 
    sem_init(&sem1, 0, 1);
    sem_init(&sem2, 0, 1);
 
    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);
 
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
 
    sem_destroy(&sem1);
    sem_destroy(&sem2);
 
    return 0;
}
 
 
