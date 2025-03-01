 
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
 
pthread_mutex_t resource;
 
void *starved_thread(void *arg) {
    while (1) {
        if (pthread_mutex_trylock(&resource) == 0) {
            printf("Starved thread got access to the resource.\n");
            sleep(1);  
            pthread_mutex_unlock(&resource);
        } else {
            printf("Starved thread waiting...\n");
            usleep(300000);
        }
    }
}
 
void *dominant_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&resource);
        printf("Dominant thread using resource.\n");
        sleep(2);  
        pthread_mutex_unlock(&resource);
        usleep(100000);
    }
}
 
int main() {
    pthread_t dominant, starved;
 
    pthread_mutex_init(&resource, NULL);
    pthread_create(&dominant, NULL, dominant_thread, NULL);
    pthread_create(&starved, NULL, starved_thread, NULL);
 
    pthread_join(dominant, NULL);
    pthread_join(starved, NULL);
    pthread_mutex_destroy(&resource);
 
    return 0;
}
 
 
