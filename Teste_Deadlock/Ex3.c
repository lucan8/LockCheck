 
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
 
pthread_mutex_t resource1, resource2;
 
void *thread1_func(void *arg) {
    pthread_mutex_lock(&resource1);
    printf("Thread 1 locked Resource 1\n");
    sleep(1);
 
    pthread_mutex_lock(&resource2);
    printf("Thread 1 locked Resource 2\n");
 
    pthread_mutex_unlock(&resource2);
    pthread_mutex_unlock(&resource1);
    return NULL;
}
 
void *thread2_func(void *arg) {
    pthread_mutex_lock(&resource2);
    printf("Thread 2 locked Resource 2\n");
    sleep(1);
 
    pthread_mutex_lock(&resource1);
    printf("Thread 2 locked Resource 1\n");
 
    pthread_mutex_unlock(&resource1);
    pthread_mutex_unlock(&resource2);
    return NULL;
}
 
int main() {
    pthread_t t1, t2;
 
    pthread_mutex_init(&resource1, NULL);
    pthread_mutex_init(&resource2, NULL);
 
    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);
 
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
 
    pthread_mutex_destroy(&resource1);
    pthread_mutex_destroy(&resource2);
 
    return 0;
}
 
