#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
 
pthread_mutex_t resource1, resource2, resource3;
 
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
 
    pthread_mutex_lock(&resource3);
    printf("Thread 2 locked Resource 3\n");
 
    pthread_mutex_unlock(&resource3);
    pthread_mutex_unlock(&resource2);
    return NULL;
}
 
void *thread3_func(void *arg) {
    pthread_mutex_lock(&resource3);
    printf("Thread 3 locked Resource 3\n");
    sleep(1);
 
    pthread_mutex_lock(&resource1);
    printf("Thread 3 locked Resource 1\n");
 
    pthread_mutex_unlock(&resource1);
    pthread_mutex_unlock(&resource3);
    return NULL;
}
 
int main() {
    pthread_t t1, t2, t3;
 
    pthread_mutex_init(&resource1, NULL);
    pthread_mutex_init(&resource2, NULL);
    pthread_mutex_init(&resource3, NULL);
 
    pthread_create(&t1, NULL, thread1_func, NULL);
    pthread_create(&t2, NULL, thread2_func, NULL);
    pthread_create(&t3, NULL, thread3_func, NULL);
 
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
 
    pthread_mutex_destroy(&resource1);
    pthread_mutex_destroy(&resource2);
    pthread_mutex_destroy(&resource3);
 
    return 0;
}
 