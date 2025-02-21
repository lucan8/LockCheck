
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
 
pthread_mutex_t resource, read_count_lock;
int read_count = 0;
 
void *reader(void *arg) {
    pthread_mutex_lock(&read_count_lock);
    read_count++;
    if (read_count == 1) {
        pthread_mutex_lock(&resource);
    }
    pthread_mutex_unlock(&read_count_lock);
 
    printf("Reader %ld is reading.\n", (long)arg);
    sleep(1);
 
    pthread_mutex_lock(&read_count_lock);
    read_count--;
    if (read_count == 0) {
        pthread_mutex_unlock(&resource);
    }
    pthread_mutex_unlock(&read_count_lock);
    return NULL;
}
 
void *writer(void *arg) {
    pthread_mutex_lock(&resource);
    printf("Writer %ld is writing.\n", (long)arg);
    sleep(2);
    pthread_mutex_unlock(&resource);
    return NULL;
}
 
int main() {
    pthread_t readers[3], writers[2];
 
    pthread_mutex_init(&resource, NULL);
    pthread_mutex_init(&read_count_lock, NULL);
 
    for (long i = 0; i < 3; i++) {
        pthread_create(&readers[i], NULL, reader, (void *)i);
    }
    for (long i = 0; i < 2; i++) {
        pthread_create(&writers[i], NULL, writer, (void *)i);
    }
 
    for (int i = 0; i < 3; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < 2; i++) {
        pthread_join(writers[i], NULL);
    }
 
    pthread_mutex_destroy(&resource);
    pthread_mutex_destroy(&read_count_lock);
 
    return 0;
}
 
