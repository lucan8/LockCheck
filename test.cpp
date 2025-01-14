#include <iostream>
#include <unistd.h>
#include <fstream>
#include <semaphore.h> 
#include <fcntl.h>
#include <sys/wait.h>
using namespace std;

const char lock_file_name[] = "lock_file.lock";

//Tries to create and open file
//On error displays error and exits(-1)
int aquire_log_lock();

//Unlinks lock file, returns 0 on succes, displays error and exit(-1) on failure 
int release_lock();

int main(){
    const char log_file_name[] = "log_file.txt";
    // Open log file and clear past contents
    ofstream clear_file(log_file_name, ifstream::out|ifstream::trunc);
    clear_file.close();
    
    // Open the file again in read mode
    ifstream fin(log_file_name);

    if(!fin){
        cerr << "Error opening log file\n";
        return -1;
    }

    pid_t pid = fork();

    //Child writes
    if (pid == 0){
        cout << "Child is executing...\n";
        char* args[] = {"test1", NULL};
        // Using the hooked functions
        char* env[] = {"LD_PRELOAD=./sys_hook.so", NULL};
        int err = execve("test1", args, env);
        if (err < 0){
            perror("Child execve");
            return -1;
        }
    }
    else{ //Parent reads
        cout << "Parent is executing...\n";
        pthread_mutex_t mutex;
        pthread_mutex_init(&mutex, NULL);
        printf("Main mutex %p\n", &mutex);
        pthread_mutex_lock(&mutex);
        printf("Locked main\n");
        pthread_mutex_unlock(&mutex);
        printf("Unlocked main\n");
        int err;
        if (wait(&err) == 0){
            perror("Wait child proccess");
            return -1;
        }
        // while(true){
        //     while(!aquire_log_lock());
            
        //     string s;
        //     getline(fin, s);
            
        //     cout << s << endl;

        //     release_lock();
        // }
    }

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