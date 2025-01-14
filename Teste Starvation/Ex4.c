 
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
 
pthread_mutex_t mutex;
 
void *worker1(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        printf("Worker 1 is working on the resource...\n");
        usleep(500000);
        pthread_mutex_unlock(&mutex);
    }
}
 
void *worker2(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        printf("Worker 2 is working on the resource...\n");
        usleep(700000);
        pthread_mutex_unlock(&mutex);
    }
}
 
int main() {
    pthread_t t1, t2;
 
    pthread_mutex_init(&mutex, NULL);
    pthread_create(&t1, NULL, worker1, NULL);
    pthread_create(&t2, NULL, worker2, NULL);
 
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_mutex_destroy(&mutex);
 
    return 0;
}
 
 