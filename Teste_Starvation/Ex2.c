#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
 
pthread_mutex_t mutex;
 
void *high_priority(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        printf("High priority thread is working...\n");
        usleep(500000);  
        pthread_mutex_unlock(&mutex);
        usleep(100000); 
    }
}
 
void *low_priority(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        printf("Low priority thread is working...\n");
        sleep(1);  
        pthread_mutex_unlock(&mutex);
    }
}
 
int main() {
    pthread_t t1, t2;
 
    pthread_mutex_init(&mutex, NULL);
    pthread_create(&t1, NULL, high_priority, NULL);
    pthread_create(&t2, NULL, low_priority, NULL);
 
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_mutex_destroy(&mutex);
 
    return 0;
}
 
