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

//TODO: Maybe write a big template function for all the functions below
// Something like:
//    - aquire lock
//    - init log file
//    - fetch real function
//    - run real function(or their try equivalent first)
//    - print approiate instruction to log file
//    - release lock
//    - return result of real function
// No error checking for pthread_mutex_lock, sem_wait and pthread_join
// Because that would require calling them before printing to file
// Which means we can't release the lock file
// Which means that if they actually have to wait they will block the lock file
static FILE* log_file = NULL;
static const char log_file_name[] = "log_file.txt";
static const char lock_file_name[] = "lock_file.lock";

void init_log_file();

//Creates and opens lock file,
//If the file already exists then the lock is already aquired so returns 0
//If the file is successfully created returns 1
//On error displays error and
int aquire_log_lock();

//Unlinks lock file, returns 0 on succes, displays error and exit(-1) on failure 
int release_lock();

// TODO: A fprintf that takes the lock, prints, calls fflush and fsync, releases the lock
void myFprintf();

int pthread_mutex_lock(pthread_mutex_t* mutex){
    // printf("WAIT MUTEX LOCK\n");
    // //Busy wait for log lock
    while(!aquire_log_lock());

    // printf("PASSED MUTEX LOCK\n");
    
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
    //Check if the mutex is blocked
    int err = pthread_mutex_trylock(mutex);
    if (err == EBUSY){
        // Print request to log file and release the lock
        fprintf(log_file, "REQUEST %ld MUTEX %p\n", pthread_self(), mutex);
        fflush(log_file);
        fsync(fileno(log_file));

        release_lock();

        //Block until mutex is free and reaquire lock to continue printing
        err = real_mutex_lock(mutex);
        while(!aquire_log_lock());
    }

    // After the mutex was taken print allocation message to log file and
    if (err == 0)
        fprintf(log_file, "ALLOCATE %ld MUTEX %p\n", pthread_self(), mutex);
    else
        perror("[ERROR] pthread_mutex_lock");
    
    //Making sure data gets to the disk
    fflush(log_file);
    fsync(fileno(log_file));
    release_lock();

    return err;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex){
    // printf("WAIT MUTEX UNLOCK\n");
    // //Busy wait for log lock
    while(!aquire_log_lock());

    // printf("PASSED MUTEX UNLOCK\n");

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

    //Making sure data gets to the disk
    fflush(log_file);
    fsync(fileno(log_file));
    release_lock();

    return 0;
}


int sem_wait(sem_t* semaphore){
    // printf("WAIT SEM WAIT\n");
    // //Busy wait for log lock
    while(!aquire_log_lock());

    // printf("PASSED SEM WAIT\n");
    
    init_log_file();

    static int (*real_sem_wait)(sem_t*) = NULL;
    if (!real_sem_wait){
        //Fetch the real sem_wait
        real_sem_wait = dlsym(RTLD_NEXT, "sem_wait");
        if (!real_sem_wait){
            printf("[ERROR]: Could not fetch the real sem_wait");
            exit(1);
        }
    }

   //Check if the semaphore is blocked
    int err = sem_trywait(semaphore);
    if (err == -1 && errno == EAGAIN){
        // Print request to log file and release the lock
        fprintf(log_file, "REQUEST %ld SEMAPHORE %p\n", pthread_self(), semaphore);
        fflush(log_file);
        fsync(fileno(log_file));

        release_lock();

        //Block until semaphore is free and reaquire lock to continue printing
        err = real_sem_wait(semaphore);
        while(!aquire_log_lock());
    }
    // After the semaphore signals print allocation message to log file and
    if (err == 0)
        fprintf(log_file, "ALLOCATE %ld SEMAPHORE %p\n", pthread_self(), semaphore);
    else
        perror("[ERROR] sem_wait");
    
    //Making sure data gets to the disk
    fflush(log_file);
    fsync(fileno(log_file));
    release_lock();

    return err;
}

int sem_post(sem_t* semaphore){
    // printf("WAIT SEM POST\n");
    // //Busy wait for log lock
    while(!aquire_log_lock());

    // printf("PASSED SEM_POST\n");

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
    
    //Making sure data gets to the disk
    fflush(log_file);
    fsync(fileno(log_file));
    release_lock();

    return 0;
}


int pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr,
                   void *(*start_routine)(void *), void *restrict arg){
    // printf("WAIT PTHREAD CREATE\n");
    // //Busy wait for log lock
    while(!aquire_log_lock());

    // printf("PASSED PTHREAD CREATE\n");

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

    //Making sure data gets to the disk
    fflush(log_file);
    fsync(fileno(log_file));
    release_lock();
    return 0;
}

int pthread_join(pthread_t thread, void** retval){
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
    
    //Joining with thread
    int err = real_pthread_join(thread, retval);

    // If join is successful aquire lock to write to log file
    if (err == 0){
        // printf("WAIT PTHREAD JOIN\n");
        //Busy wait for log lock
        while(!aquire_log_lock());

        // printf("PASSED PTHREAD JOIN\n");

        //Print the request to the log file
        fprintf(log_file, "DESTROY THREAD %ld\n", thread);

        release_lock();
    }
    else{
        printf("Failed pthread join %ld", pthread_self());
    }

    //Making sure data gets to the disk
    fflush(log_file);
    fsync(fileno(log_file));
    return err;
}


int pthread_mutex_init(pthread_mutex_t* restrict mutex,
                       const pthread_mutexattr_t* restrict attr){
    // printf("WAIT MUTEX INIT\n");
    // //Busy wait for log lock
    while(!aquire_log_lock());

    // printf("PASSED MUTEX INIT\n");

    init_log_file();
    
    static int (*real_mutex_init)(pthread_mutex_t* restrict,
                                  const pthread_mutexattr_t* restrict);
    if (!real_mutex_init){
        //Fetch the real pthread_create
        real_mutex_init = dlsym(RTLD_NEXT, "pthread_mutex_init");

        //Check if mutex_init was found
        if (!real_mutex_init){
            printf("Err: Could not fetch the real pthread_mutex_init");
            exit(1);
        }
    }

    //Checking if the mutex was initialized successfully
    int err = real_mutex_init(mutex, attr);
    if (err){
        release_lock();
        return err;
    }

    //Print the request to the log file
    fprintf(log_file, "CREATE MUTEX %p\n", mutex);

    //Making sure data gets to the disk
    fflush(log_file);
    fsync(fileno(log_file));
    release_lock();
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t* mutex){
    // printf("WAIT MUTEX DESTROY\n");
    // //Busy wait for log lock
    while(!aquire_log_lock());

    // printf("PASSED MUTEX DESTROY\n");

    init_log_file();
    
    static int (*real_mutex_destroy)(pthread_mutex_t*);
    if (!real_mutex_destroy){
        //Fetch the real pthread_create
        real_mutex_destroy = dlsym(RTLD_NEXT, "pthread_mutex_destroy");
        if (!real_mutex_destroy){
            printf("Err: Could not fetch the real pthread_mutex_destroy");
            exit(1);
        }
    }

    //Checking if the mutex was destroyed successfully
    int err = real_mutex_destroy(mutex);
    if (err){
        release_lock();
        return err;
    }

    //Print the instruction to the log file
    fprintf(log_file, "DESTROY MUTEX %p\n", mutex);

    //Making sure data gets to the disk
    fflush(log_file);
    fsync(fileno(log_file));
    release_lock();
    return 0;
}

int sem_init(sem_t* semaphore, int pshared, unsigned int value){
    // printf("WAIT SEM INIT\n");
    // //Busy wait for log lock
    while(!aquire_log_lock());

    // printf("PASSED SEM INIT\n");

    init_log_file();
    
    static int (*real_sem_init)(sem_t*, int, unsigned int);
    if (!real_sem_init){
        //Fetch the real pthread_create
        real_sem_init = dlsym(RTLD_NEXT, "sem_init");
        if (!real_sem_init){
            printf("Err: Could not fetch the real sem_init");
            exit(1);
        }
    }

    //Checking if the semaphore was initialized successfully
    int err = real_sem_init(semaphore, pshared, value);
    if (err){
        release_lock();
        return err;
    }

    //Print the instruction to log file
    fprintf(log_file, "CREATE SEMAPHORE %p %u\n", semaphore, value);

    //Making sure data gets to the disk
    fflush(log_file);
    fsync(fileno(log_file));
    release_lock();
    return 0;
}

int sem_destroy(sem_t* semaphore){
    // printf("WAIT SEM DESTROY\n");
    // //Busy wait for log lock
    while(!aquire_log_lock());

    // printf("PASSED SEM DESTROY\n");

    init_log_file();
    
    static int (*sem_destroy)(sem_t*);
    if (!sem_destroy){
        //Fetch the real pthread_create
        sem_destroy = dlsym(RTLD_NEXT, "sem_destroy");
        if (!sem_destroy){
            printf("Err: Could not fetch the real sem_destroy");
            exit(1);
        }
    }

    //Checking if the semaphore was destroyed successfully
    int err = sem_destroy(semaphore);
    if (err){
        release_lock();
        return err;
    }

    //Print the instruction to the log file
    fprintf(log_file, "DESTROY SEMAPHORE %p\n", semaphore);

    //Making sure data gets to the disk
    fflush(log_file);
    fsync(fileno(log_file));
    release_lock();
    return 0;
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
    printf("Suspect file: Lock aquired\n");
    close(fd);
    return 1;
}

int release_lock(){
    int err = unlink(lock_file_name);
    if (err){
        perror("Suspect file: Release lock unlink");
        exit(-1);
    }
    printf("Suspect file: Released lock\n");
    return 0;
}