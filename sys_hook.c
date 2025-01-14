#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>

//TODO: Add functions for sem_init and pthread_mutex_init(CREATE MUTEX/SEMAPHORE obj_ptr)
// No error checking for pthread_mutex_lock, sem_wait and pthread_join
// Because that would require calling them before printing to file
// Which means we can't release the lock file
// Which means that if they actually have to wait they will block the lock file
static FILE* log_file = NULL;
static const char log_file_name[] = "log_file.txt";
const char lock_file_name[] = "lock_file.lock";

void init_log_file();

//Creates and opens lock file,
//If the file already exists then the lock is already aquired so returns 0
//If the file is successfully created returns 1
//On error displays error and
int aquire_log_lock();

//Unlinks lock file, returns 0 on succes, displays error and exit(-1) on failure 
int release_lock();


int pthread_mutex_lock(pthread_mutex_t* mutex){
    printf("WAIT MUTEX LOCK\n");
    //Busy wait for log lock
    while(!aquire_log_lock());

    printf("PASSED MUTEX LOCK\n");
    
    init_log_file();

    static int (*real_mutex_lock)(pthread_mutex_t*) = NULL;
    if (!real_mutex_lock){
        //Fetch the real pthread_mutex_lock
        real_mutex_lock = dlsym(RTLD_NEXT, "pthread_mutex_lock");
        if (!real_mutex_lock){
            printf("Err: Could not fetch the real pthread_mutex_lock");
            exit(1);
        }
    }
    
    //Print the request to the log file
    fprintf(log_file, "REQUEST %ld MUTEX %p\n", pthread_self(), mutex);
    release_lock();

    return real_mutex_lock(mutex);
}

int pthread_mutex_unlock(pthread_mutex_t* mutex){
    printf("WAIT MUTEX UNLOCK\n");
    //Busy wait for log lock
    while(!aquire_log_lock());

    printf("PASSED MUTEX UNLOCK\n");

    init_log_file();

    static int (*real_mutex_unlock)(pthread_mutex_t*) = NULL;
    if (!real_mutex_unlock){
        //Fetch the real pthread_mutex_lock
        real_mutex_unlock = dlsym(RTLD_NEXT, "pthread_mutex_unlock");
        if (!real_mutex_unlock){
            printf("Err: Could not fetch the real pthread_mutex_unlock");
            exit(1);
        }
    }

    // Making sure real_mutex_unlock returned successfully
    int err = real_mutex_unlock(mutex);
    if (err != 0){
        release_lock();
        return err;
    }

    //Print the request to the log file
    fprintf(log_file, "RELEASE %ld MUTEX %p\n", pthread_self(), mutex);
    release_lock();

    return 0;
}


int sem_wait(sem_t* semaphore){
    printf("WAIT SEM WAIT\n");
    //Busy wait for log lock
    while(!aquire_log_lock());

    printf("PASSED SEM WAIT\n");
    
    init_log_file();

    static int (*real_sem_wait)(sem_t*) = NULL;
    if (!real_sem_wait){
        //Fetch the real sem_wait
        real_sem_wait = dlsym(RTLD_NEXT, "sem_wait");
        if (!real_sem_wait){
            printf("Err: Could not fetch the real sem_wait");
            exit(1);
        }
    }

    //Print the request to the log file
    fprintf(log_file, "REQUEST %ld SEMAPHORE %p\n", pthread_self(), semaphore);
    release_lock();

    return real_sem_wait(semaphore);
}

int sem_post(sem_t* semaphore){
    printf("WAIT SEM POST\n");
    //Busy wait for log lock
    while(!aquire_log_lock());

    printf("PASSED SEM_POST\n");

    init_log_file();

    static int (*real_sem_post)(sem_t*) = NULL;
    if (!real_sem_post){
        //Fetch the real sem_post
        real_sem_post = dlsym(RTLD_NEXT, "sem_post");
        if (!real_sem_post){
            printf("Err: Could not fetch the real sem_post");
            exit(1);
        }
    }

    // Making sure real_sem_post returned successfully
    int err = real_sem_post(semaphore);
    if (err != 0){
        release_lock();
        return err;
    }

    //Print the request to the log file
    fprintf(log_file, "RELEASE %ld SEMAPHORE %p\n", pthread_self(), semaphore);
    release_lock();

    return 0;
}


int pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr,
                   void *(*start_routine)(void *), void *restrict arg){
    printf("WAIT PTHREAD CREATE\n");
    //Busy wait for log lock
    while(!aquire_log_lock());

    printf("PASSED PTHREAD CREATE\n");

    init_log_file();
    
    static int (*real_pthread_create)(pthread_t *restrict,
                                      const pthread_attr_t *restrict,
                                      void *(*)(void *),
                                      void *restrict) = NULL;
    if (!real_pthread_create){
        //Fetch the real pthread_create
        real_pthread_create = dlsym(RTLD_NEXT, "pthread_create");
        if (!real_pthread_create){
            printf("Err: Could not fetch the real pthread_create");
            exit(1);
        }
    }

    //Checking if the thread was created successfully
    int err = real_pthread_create(thread, attr, start_routine, arg);
    if (err){
        release_lock();
        return err;
    }

    //Print the request to the log file
    fprintf(log_file, "CREATE THREAD %ld\n", *thread);

    release_lock();
    return 0;
}

int pthread_join(pthread_t thread, void** retval){
    printf("WAIT PTHREAAD JOIN\n");
    //Busy wait for log lock
    while(!aquire_log_lock());

    printf("PASSED PTHREAD JOIN\n");
    
    init_log_file();

    static int (*real_pthread_join)(pthread_t, void**) = NULL;
    if (!real_pthread_join){
        //Fetch the real pthread_join
        real_pthread_join = dlsym(RTLD_NEXT, "pthread_join");
        if (!real_pthread_join){
            printf("Err: Could not fetch the real pthread_join");
            exit(1);
        }
    }

    //Print the request to the log file
    fprintf(log_file, "DESTROY THREAD %ld\n", thread);

    release_lock();
    return real_pthread_join(thread, retval);
}


void init_log_file(){
    if (log_file != NULL)
        return;

    //Open and create file for appending the messages
    printf("Initializing log file\n");
    log_file = fopen(log_file_name, "a");
    
    //CHecking if file was actually opened successfully
    if (log_file == NULL){
        perror("Opening log_file failed from sys_hook");
        exit(1);
    }

    printf("Succesfully initialized\n");
}

int aquire_log_lock(){
    int fd = open(lock_file_name, O_CREAT|O_EXCL, 0644);
    if (fd < 0){
        if (errno == EEXIST)
            return 0;
        perror("aquire_log_lock file open");
        exit(-1);
    }
    printf("Lock aquired\n");
    close(fd);
    return 1;
}

int release_lock(){
    int err = unlink(lock_file_name);
    if (err){
        perror("Release lock unlink");
        exit(-1);
    }
    return 0;
}