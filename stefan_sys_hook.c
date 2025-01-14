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
const char lock_file_name[] = "lock_file.lock";

void init_log_file();

//Creates and opens lock file,
//If the file already exists then the lock is already aquired so returns 0
//If the file is successfully created returns 1
//On error displays error and
int aquire_log_lock();

//Unlinks lock file, returns 0 on succes, displays error and exit(-1) on failure
int release_lock();
struct arguments
{
    void* data;
};
int manage_res (char* res_name, void* res, const char* instruction, struct arguments* args);

int pthread_mutex_lock(pthread_mutex_t* mutex)
{
    struct arguments args [] =
    {
       { (void*)mutex }
    };
    return manage_res("pthread_mutex", mutex, "lock", args);
}

int pthread_mutex_unlock(pthread_mutex_t* mutex)
{
    struct arguments args [] =
    {
       { mutex }
    };
    return manage_res("pthread_mutex", mutex, "unlock", args);
}


int sem_wait(sem_t* semaphore)
{
    struct arguments args [] =
    {
        semaphore
    };
    return manage_res("sem", semaphore, "wait", args);
}

int sem_post(sem_t* semaphore)
{
    struct arguments args [] =
    {
        semaphore
    };
    return manage_res("sem", semaphore, "post", args);
}


int pthread_create(pthread_t *restrict thread, const pthread_attr_t *restrict attr,
                   void *(*start_routine)(void *), void *restrict arg)
{
    struct arguments args [] =
    {
        thread,
        attr,
        start_routine,
        arg
    };
    return manage_res("pthread", thread, "create", args);
}

int pthread_join(pthread_t thread, void** retval)
{
    struct arguments args [] =
    {
        thread,
        retval
    };
    return manage_res("pthread", thread, "join", args);
}


int pthread_mutex_init(pthread_mutex_t* restrict mutex,
                       const pthread_mutexattr_t* restrict attr)
{
    struct arguments args [] =
    {
        mutex,
        attr
    };
    return manage_res("pthread_mutex", mutex, "init", args);
}

int pthread_mutex_destroy(pthread_mutex_t* mutex)
{
    struct arguments args [] =
    {
        mutex
    };
    return manage_res("pthread_mutex", mutex, "destroy", args);
}

int sem_init(sem_t* semaphore, int pshared, unsigned int value)
{
    struct arguments args [] =
    {
        semaphore,
        (void*) pshared,
        value
    };
    return manage_res("sem", semaphore, "init", args);
}

int sem_destroy(sem_t* semaphore)
{
    struct arguments args [] =
    {
        semaphore,
    };
    return manage_res("sem", semaphore, "destroy", args);
}

void init_log_file()
{
    if (log_file != NULL)
        return;

    //Open and create file for appending the messages
    printf("Initializing log file\n");
    log_file = fopen(log_file_name, "a");

    //CHecking if file was actually opened successfully
    if (log_file == NULL)
    {
        perror("Opening log_file failed from sys_hook");
        exit(1);
    }

    printf("Succesfully initialized\n");
}

int aquire_log_lock()
{
    int fd = open(lock_file_name, O_CREAT|O_EXCL, 0644);
    if (fd < 0)
    {
        if (errno == EEXIST)
            return 0;
        perror("aquire_log_lock file open");
        exit(-1);
    }
    printf("Lock aquired\n");
    close(fd);
    return 1;
}

int release_lock()
{
    int err = unlink(lock_file_name);
    if (err)
    {
        perror("Release lock unlink");
        exit(-1);
    }
    printf("Released lock\n");
    return 0;
}
int manage_res (char* res_name, void* res, const char* instruction, struct arguments* args)
{
    printf("WAIT %s %s\n",res_name, instruction);
    //Busy wait for log lock
    while(!aquire_log_lock());

    printf("PASSED %s\n", instruction);

    init_log_file();
    char func_name[50];
    snprintf(func_name, sizeof(func_name), "%s_%s", res_name, instruction);

    char func_try_name[50];
    snprintf(func_try_name, sizeof(func_try_name), "%s_try%s", res_name, instruction);
    static int (*real_function)(void*) = NULL;
    if (!real_function)
    {
        //Fetch the real function
        real_function = dlsym(RTLD_NEXT, func_name);
        if (!real_function)
        {
            printf("Err: Could not fetch the real function");
            exit(1);
        }
    }

    char log_keyword[20];
    int err = 0;
    if(strcmp(instruction, "lock") == 0 || strcmp(instruction, "wait") == 0)
    {
        static int (*try_function)(void*) = NULL;
	if (!try_function)
	    {
		//Fetch the try function
		try_function = dlsym(RTLD_NEXT, func_try_name);
		if (!try_function)
		{
		    printf("Err: Could not fetch the try function");
		    exit(1);
		}
	    }
        err = try_function(args);
        if (err == EBUSY || err == EAGAIN)
        {
            // Print request to log file and release the resource
            fprintf(log_file, "REQUEST %ld %s %p\n", pthread_self(),res_name, res);
            release_lock();
            //Block until mutex is free and reaquire lock to continue printing
            err = real_function(args);
            while(!aquire_log_lock());
        }
        strcpy(log_keyword, "ALLOCATE");

    }
    else
    {
        if(strcmp(instruction, "join") == 0)
        {
            fprintf(log_file, "DESTROY THREAD %p\n", res);
            release_lock();
            return real_function(args);
        }
        else
        {
            // Making sure real_function returned successfully
            if(strcmp(instruction, "create") == 0)
            {
                err = real_function(args);
                strcpy(log_keyword, "CREATE");
            }
            if(strcmp(instruction, "init") == 0)
            {
                if(strcmp(res_name, "pthread_mutex") == 0)
                    err = real_function(args);
                else
                    err = real_function(args);
                strcpy(log_keyword, "CREATE");
            }
            if(strcmp(instruction, "destroy") == 0)
            {
                err = real_function(args);
                strcpy(log_keyword, "DESTROY");
            }
            if(strcmp(instruction, "unlock") == 0 || strcmp(instruction, "post") == 0)
            {
                err = real_function(args);
                strcpy(log_keyword, "RELEASE");
            }
            if (err != 0)
            {
                release_lock();
                return err;
            }
            err = 0;
        }
    }
    if (err == 0)
        fprintf(log_file,"%s %ld %s %p\n",log_keyword, pthread_self(), res_name, res);
    release_lock();
    return err;
}
