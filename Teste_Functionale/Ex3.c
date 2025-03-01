 
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
 
pthread_mutex_t mutex;
pthread_cond_t cond;
int ready = 0;
 
void *worker(void *arg) {
    pthread_mutex_lock(&mutex);
    ready++;
    printf("Thread %ld ready.\n", (long)arg);
 
    if (ready == 3) {
        pthread_cond_broadcast(&cond);
    } else {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    printf("Thread %ld proceeding.\n", (long)arg);
 
    return NULL;
}
 
int main() {
    pthread_t threads[3];
 
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
 
    for (long i = 0; i < 3; i++) {
        pthread_create(&threads[i], NULL, worker, (void *)i);
    }
 
    for (int i = 0; i < 3; i++) {
        pthread_join(threads[i], NULL);
    }
 
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
 
    return 0;
}
