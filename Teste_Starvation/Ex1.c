#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t mutex;

void *high_priority_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&mutex);
        printf("High priority thread is working...\n");
        sleep(1);  
        pthread_mutex_unlock(&mutex);
        usleep(100000); 
    }
    return NULL;
}

void *low_priority_thread(void *arg) {
    while (1) {
        if (pthread_mutex_trylock(&mutex) == 0) {
            printf("Low priority thread is working...\n");
            sleep(1);  
            pthread_mutex_unlock(&mutex);
        } else {
            printf("Low priority thread is waiting...\n");
            usleep(500000);
        }
    }
    return NULL;
}

int main() {
    pthread_t high, low;

    pthread_mutex_init(&mutex, NULL);

    pthread_create(&high, NULL, high_priority_thread, NULL);
    pthread_create(&low, NULL, low_priority_thread, NULL);

    pthread_join(high, NULL);
    pthread_join(low, NULL);

    pthread_mutex_destroy(&mutex);
    return 0;
}












