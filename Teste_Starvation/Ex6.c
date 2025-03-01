#include <pthread.h>
#include <stdio.h>

pthread_mutex_t mutex;

void* threadFunc(void* arg) {
    pthread_mutex_lock(&mutex);
    printf("Thread %d in critical section.\n", *(int*)arg);
    return NULL;
}

int main() {
    pthread_mutex_init(&mutex, NULL);

    pthread_t thread1, thread2;
    int id1 = 1, id2 = 2;
    pthread_create(&thread1, NULL, threadFunc, &id1);
    pthread_create(&thread2, NULL, threadFunc, &id2);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    pthread_mutex_destroy(&mutex);
    return 0;
}
