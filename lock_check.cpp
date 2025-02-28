#include <iostream>
#include <vector>
#include <sstream>
#include <pthread.h>
#include <semaphore.h>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <algorithm>
#include <exception>
#include <cstring>
#include <string>
#include <filesystem>
#include <elf.h>
using namespace std;

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)


// Deadlock detection seems to work(finally)
// For now we are just killing the process and printing
// We need to think about recovery now

//TODO: Write unsafe thread exception
// I also found that this algorithm can detect both deadlocks and starvation
// But further thinking is needed to differentiate between the two
// TODO: Make abstract class Object that has an id
// TODO: We don't actually need to reallocate from requested before
// freeing a safe thread, but we need to modify the destructor
// to check for requested

static const char lock_file_name[] = "lock_file.lock";
// Classes used for deadlock detection
class DetectionResource;
class DetectionThread;
class DetectionAlgo;

//User defined Exceptions
class Myexception;
class FullResourceException;
class EmptyResourceException;
class ResourceNotFoundException;

typedef shared_ptr<DetectionResource> shared_det_res;
typedef unique_ptr<DetectionThread> unique_det_thread;
typedef shared_ptr<DetectionThread> shared_det_thread;


enum ResourceTypes{
    MUTEX = 0,
    SEMAPHORE = 1,
};

// Not used at the moment
enum MessageTypes{
    OK,
    WARNING,
    ERROR,
    DEBUG
};

string toString(ResourceTypes res_type);
string toString(MessageTypes msg_type); //Not used at the moment

vector<string> split(const string& input_file);
off_t getFileSize(const string& file_name);

//Tries to create and open file
//On error displays error and exits(-1)
int aquire_log_lock();

//Unlinks lock file, returns 0 on succes, displays error and exit(-1) on failure 
int release_lock();

bool fileExists(const string& file_name);

// Looks at the file header and checks whether it is an ELF executable
bool isElfExecutable(const char* filepath);

// Checks for user input and returns an error string if an error occurs, otherwise "" is returned
string validateInput(int argc, char* argv[]);

class MyException : public exception{
protected:
    string file_name, func_name, line, msg;
public:
    MyException(const string& file_name, const string& func_name, int line)
        : file_name(file_name), func_name(func_name), line(to_string(line)){
            msg = "In " + file_name + ", function " + func_name + " , line " + this->line + ": ";
        }
    virtual const char* what() const noexcept {
        return msg.c_str();
    }
};

class FileExistsException: public MyException{
protected:
    string open_f_name;
public:
    FileExistsException(const string& file_name, const string& func_name, int line, const string& open_f_name)
        : MyException(file_name, func_name, line), open_f_name(open_f_name){
            msg += "File is already open " + open_f_name;
        }
    virtual const char* what() const noexcept {
        return msg.c_str();
    }
};

class FileOpenException : public MyException{
protected:
    string open_f_name;
public:
    FileOpenException(const string& file_name, const string& func_name, int line, const string& open_f_name)
        : MyException(file_name, func_name, line), open_f_name(open_f_name){
            msg += "Could not open file " + open_f_name;
        }
    virtual const char* what() const noexcept {
        return msg.c_str();
    }
};

class FullResourceException : public MyException{
private:
   const string& res_id;
   string res_type;
public:
    FullResourceException(const string& file_name, const string& func_name, int line,  const string& res_id, ResourceTypes res_type)
    : MyException(file_name, func_name, line), res_id(res_id), res_type(toString(res_type)){
        msg += " resource " + res_id + " " + this->res_type + " is full";
    }
};

class EmptyResourceException : public MyException{
private:
    const string& res_id;
    string res_type;
public:
EmptyResourceException(const string& file_name, const string& func_name, int line,  const string& res_id, ResourceTypes res_type)
    : MyException(file_name, func_name, line), res_id(res_id), res_type(toString(res_type)){
        msg += " resource " + res_id + " " + this->res_type + " is empty";
    }
};

class ResourceNotFoundException: public MyException{
private:
    const string& res_id;
public:
    ResourceNotFoundException(const string& file_name, const string& func_name, int line, const string& res_id)
    : MyException(file_name, func_name, line), res_id(res_id){
        msg += " resource " + res_id + " not found";
    }
};

// Simple wrapper class for ofstream that also keeps track of the file_name
class MyOutFile{
private:
    ofstream stream;
    string file_name;
public:
    MyOutFile(const string& file_name): file_name(file_name), stream(file_name){}
    
    const string& getFileName() const {return file_name;}
    ofstream& getStream() {return stream;}

    void flush(){stream.flush();}
};

class DetectionResource{
private:
    string id;
    ResourceTypes res_type;
    size_t count;
    size_t max_count;
public:
    DetectionResource(const string& id, const ResourceTypes& res_type, size_t count = 1)
        : res_type(res_type), count(count), max_count(count), id(id){
        
        //Mutexes are just semaphores with count of 1
        if (res_type == ResourceTypes::MUTEX)
            count = 1;
    }

    ResourceTypes getResType() const {return res_type; }
    size_t getCount() const {return count;}
    size_t getMaxCount() const {return max_count;}
    const string& getId() const {return id;}

    // Throws error is resource is full
    void increaseCount() {
        if (isFull())
            throw FullResourceException(__FILENAME__, __func__, __LINE__, this->id, this->res_type);

        count++;
    }

    // Throws error if resource is locked
    void decreaseCount(){
        if (isLocked())
            throw EmptyResourceException(__FILENAME__, __func__, __LINE__, this->id, this->res_type);
        
        count--;
    }

    bool isLocked(){return count <= 0;}
    bool isFull() {return count >= max_count;}

    friend ostream& operator<<(ostream& out, const DetectionResource& res);
};

class DetectionThread{
private:
    string id;
    unordered_map<string, shared_det_res>allocated_res;
    unordered_map<string, shared_det_res>requested_res;
    unordered_map<string, pair<shared_det_res, int>> res_counter;

    // Tries to allocate the resource, throws error if resource is locked
    void _allocate(const string& res_id,  shared_det_res res){
        res->decreaseCount();
        allocated_res.insert(make_pair(res_id, move(res)));
    }

    // Moves the resource from requested to allocated
    // Can throw if the resource is locked or not found in the requested container
    void _allocateFromRequested(const shared_det_res& res){
        if (res->isLocked())
            throw EmptyResourceException(__FILENAME__, __func__, __LINE__, res->getId(), res->getResType());
        
        // Trying to extract resource only after we are sure it's not locked
        auto found_res = requested_res.extract(res->getId());
        if (found_res.empty())
            throw ResourceNotFoundException(__FILENAME__, __func__, __LINE__, this->id);
        
        // Will do an extra isLocked check unfortunately
        this->_allocate(move(found_res.key()), move(found_res.mapped()));
    }

    // Helper function for the two public releases()
    // Increases the count on the resource an prints appropriate message to console
    // Can throw error if resource is full
    void _release (DetectionResource& res){
        // Increase count for resource
        res.increaseCount();
        cout << "Thread " << this->id << " released " << res << '\n';

        // res_counter[res_id].second += 1;

        // cout << "After release res count: " << res_counter[res_id].second << endl;
       
    }
public:
    DetectionThread(const string& id): id(id){}
    ~DetectionThread(){
        this->release();
    }

    const unordered_map<string, shared_det_res>& getAllocatedRes() const {return allocated_res;}
    const unordered_map<string, shared_det_res>& getRequestedRes() const {return requested_res;}
    const string& getId() const {return id;}

    bool hasResource(const string& res_id) const { return allocated_res.find(res_id) != allocated_res.end();}
    bool requestsResource(const string& res_id) const { return requested_res.find(res_id) != requested_res.end();}
    
    void allocateResCounter(const string& res_id, shared_det_res res){
        // Keeping track of the number of locks and releases
        // Made by this thread on the resource res_id
        if(res_counter.find(res_id) == res_counter.end())
            res_counter[res_id] = make_pair(res, -1);
        else
            res_counter[res_id].second -= 1;
    }

    // Tries to allocate from requested
    // If not found in the request vector allocates normally
    // Throws error if the resource is locked
    void allocate(const string& res_id, shared_det_res res){
        // allocateResCounter(res_id, res);
        // cout << "After allocate res count: " << res_counter[res_id].second << endl;
        
        // First try to allocate from requested, if not found allocate the resource normally
        try{
            cout << "Moving from requested\n";
            this->_allocateFromRequested(res);
        } catch (ResourceNotFoundException& e){
            cout << "Move failed. Allocate normally\n";
            this->_allocate(res_id, move(res));
        }
    }

    // Moves as much as possible from requested to allocated
    void allocateFromRequested(){
        for (auto it = requested_res.begin(); it != requested_res.end();){
            // Ignoring locked resources
            if (it->second->isLocked())
                continue;
            
            // Moving resource from requested to allocated
            auto res = requested_res.extract(it++);
            this->_allocate(move(res.key()), move(res.mapped()));
        }
    }

    void request(const string& res_id, shared_det_res res){
        requested_res.insert(make_pair(res_id, move(res)));
    }
    
    
    // Releases resource with res_id
    // Throws error if not found or resource is full
    void release(const string& res_id){
        // cout << "Small release " << res_id << '\n';

        // Remove and store the resource
        auto found_res = allocated_res.extract(res_id);
        if (found_res.empty())
            throw ResourceNotFoundException(__FILENAME__, __func__, __LINE__, res_id);

        this->_release(*found_res.mapped());
    }


    //Releases all allocated resources
    void release(){
        for (auto it = allocated_res.begin(); it != allocated_res.end();){
            // Extract the resource node from map and move to the next resource
            auto res = allocated_res.extract(it++);
            // Release the resource
            this->_release(*res.mapped());
        }
    }

    // True if no requests exist, false otherwise
    bool canExit() const{
        return requested_res.empty();
    }

    // True if there is no locked resource in the requested container, false otherwise
    bool canFulfillAllReq(){
        for (const auto& r: requested_res)
            if (r.second->isLocked())
                return false;
        return true;
    }

    bool check_resources(){
        for (auto counter : res_counter){
            if(counter.second.second != 0){
                cout << "Thread " << counter.first << "has resource count "
                     << counter.second.second << endl;
                return false;
            }
        }
        return true;
    }
    
    // Compares threads by their number of requests
    friend bool compByNrReq(const DetectionThread& t1, const DetectionThread& t2);
};

class DetectionAlgo{
protected:
    unordered_map<string, shared_det_thread> threads;
    unordered_map<string, shared_det_res> resources;
    shared_ptr<MyOutFile> deadlk_check_file;

    // Helper functions for public "parse"
    void parseCreate(const string& obj_type, const string& obj_id,
                     const string& res_count = ""){
        if (obj_type == "THREAD")
            addThread(obj_id);
        else if (obj_type == "MUTEX")
            addResource(obj_id, ResourceTypes::MUTEX, 1);
        else if (obj_type == "SEMAPHORE")
            addResource(obj_id, ResourceTypes::SEMAPHORE, stoul(res_count));
    }

    void parseDestroy(const string& obj_type, const string& obj_id){
        if (obj_type == "THREAD")
            removeThread(obj_id);
        else if (obj_type == "MUTEX" || obj_type == "SEMAPHORE")
            removeResource(obj_id);
    }

    // Allocates resource to thread and checks for deadlock
    void parseAllocate(const string& thread_id, const string& res_type,
                       const string& res_id){
        try{
            threads.at(thread_id)->allocate(res_id, resources.at(res_id));
            cout << "Allocate " << res_type << " for Thread: " 
                 << thread_id << " successfull" << endl;
        } catch(out_of_range& e){
            cout << "[ERROR] Allocate: " << res_type << " Thread not found: " 
                 << thread_id << endl;
        }
        // catch (EmptyResourceException& e){
        //     cout << "[ERROR]" << e.what() << '\n';
        // }

        // if (!isSafeState())
        //     throw runtime_error("DEADLOCK DETECTED");

    }

    // Sets request for resource to thread and checks for deadlock
    void parseRequest(const string& thread_id, const string& res_type,
                       const string& res_id){
         try{
            threads.at(thread_id)->request(res_id, resources.at(res_id));
            cout << "Request " << res_type << " for Thread: " 
                 << thread_id << " successfull" << endl;
        } catch(out_of_range& e){
            cerr << "Request " << res_type << " Thread not found: " 
                 << thread_id << endl;
        }

        cout << "Checking for deadlock...\n";
        if (!isSafeState())
            throw runtime_error("DEADLOCK DETECTED");
        cout << "No deadlock found\n";
    }

    void parseRelease(const string& thread_id, const string& res_type,
                       const string& res_id){
        try{
            threads.at(thread_id)->release(res_id);
            cerr << "Release " << res_type << " for Thread: " 
                 << thread_id << " successfull" << endl;
        } catch(out_of_range& e){
            cerr << "Release " << res_type << " Thread not found: " 
                 << thread_id << endl;
        } 
    }

    //Helper function for public "clone"
    void cloneResources(DetectionAlgo& clone_algo){
        ofstream& temp_stream = deadlk_check_file->getStream();

        temp_stream << "   Cloning resources...\n";
        // Clone resources and restart their count for reallocation
        for (const auto& r: resources){
            const string& res_id = r.first;
            const shared_det_res& res = r.second;

            clone_algo.addResource(res_id, res->getResType(), res->getMaxCount());
            temp_stream << "       Cloned " << *clone_algo.resources[res_id] << "\n";
        }
        temp_stream << "   Cloned " << resources.size() << " resources\n";
    }

    void cloneThreads(DetectionAlgo& clone_algo){
        ofstream& temp_stream = deadlk_check_file->getStream();

        temp_stream << "   Cloning threads...\n";
        for (const auto& t: threads){
            // Create thread with identical id
            const string& t_id = t.first;
            const shared_det_thread& thread = t.second;
            clone_algo.addThread(t_id);

            // Get the just cloned thread and clone it's allocations and requests
            shared_det_thread& clone_thread = clone_algo.threads[t_id];

            temp_stream << "        Thread " << t_id << ":\n";
            cloneThreadAllocations(clone_thread, thread->getAllocatedRes(), clone_algo);
            cloneThreadRequests(clone_thread, thread->getRequestedRes(), clone_algo);
        }
        temp_stream << "   Cloned " << threads.size() << " threads\n";
    }

    void cloneThreadAllocations(shared_det_thread& clone_thread,
                                const unordered_map<string, shared_det_res>& cloned_res,
                                DetectionAlgo& clone_algo){
        
        ofstream& temp_stream = deadlk_check_file->getStream();
        // Going through the thread's allocated resources
        // And referencing their clones
        temp_stream << "           Cloning thread allocations...\n";
        for (const auto& t_r: cloned_res){
            temp_stream << "               Resource: " << *(t_r.second) << '\n';
            
            const string& t_res_id = t_r.first;
            const shared_det_res& t_clone_res = clone_algo.resources.at(t_res_id);
        
            clone_thread->allocate(t_res_id, t_clone_res);
        }
        temp_stream << "           Cloned " << cloned_res.size() << " allocations\n";
    }

    void cloneThreadRequests(shared_det_thread& clone_thread,
                             const unordered_map<string, shared_det_res>& cloned_res,
                             DetectionAlgo& clone_algo){
        ofstream& temp_stream = deadlk_check_file->getStream();

        //Going through the thread's requested resources
        // and referencing their clones
        temp_stream << "           Cloning Thread requests...\n";
        for (const auto& t_r: cloned_res){
            temp_stream << "                   Resource: " << *(t_r.second) << '\n';
            const string& t_res_id = t_r.first;
            const shared_det_res& t_clone_res = clone_algo.resources.at(t_res_id);
            clone_thread->request(t_res_id, t_clone_res);
        }
        temp_stream << "           Cloned " << cloned_res.size() << " requests\n";
    }

    // For the clone, it doesn't need the file
    DetectionAlgo(){}
public:
    DetectionAlgo(const string& file_name){
        // Making sure the file gets created now if needed for writing
        // if (fileExists(file_name))
        //     throw FileExistsException(__FILENAME__, __func__, __LINE__, file_name);
        this->deadlk_check_file = make_shared<MyOutFile>(file_name);
        this->deadlk_check_file->getStream() << "File Created\n";
    }

    ~DetectionAlgo(){}

    bool detect(){}

    // Clones threads(including allocations and requests) and resources
    // Shares the output file
    DetectionAlgo clone(){
        deadlk_check_file->getStream() << "Cloning deadlock algo...\n";
        DetectionAlgo clone_algo;

        cloneResources(clone_algo);
        cloneThreads(clone_algo);

        deadlk_check_file->flush();

        return clone_algo;
    }

    // Frees safe threads until either there is only one thread remaining or no safe threads exist
    bool isSafeState(){
        // Getting current state of the algorithm
        DetectionAlgo clone_algo = clone();

        ofstream& temp_stream = this->deadlk_check_file->getStream(); 
        
        temp_stream << "isSafeState: \n";
        int i = 0;

        // An iteration is safe only if there exists at least one thread that can fulfill all it's requests
        bool is_safe = true;
        
        while (clone_algo.threads.size() > 1 && is_safe){
            temp_stream << "   Iteration " << i << '\n';

            is_safe = false;
            // Removing the threads that can fullfill all their requests and freeing their resources
            for (auto it = clone_algo.threads.begin(); it != clone_algo.threads.end();)
                if (it->second->canFulfillAllReq()){
                    temp_stream << "       Removing thread " << it->first << '\n';
                    is_safe = true;
                    it->second->allocateFromRequested();
                    clone_algo.threads.erase(it++);
                }
                else
                    it++;
            
            temp_stream << "       Remaining threads: " << clone_algo.threads.size() << '\n';
            temp_stream << "   Iteration " << i << " finished\n";
            i++;
        }
        deadlk_check_file->flush();

        return is_safe;
    }

    void addThread(const string& t_id){
        threads.insert(make_pair(t_id, make_shared<DetectionThread>(t_id)));

        // if (threads.at(t_id)->check_resources())
        cout << "Deadlock detection: Created thread " << t_id << endl;
    }

    void addResource(const string& res_id, ResourceTypes res_type, unsigned int count){
        resources.insert(make_pair(res_id,
                                   make_shared<DetectionResource>(res_id, res_type, count)));
        
        cout << "Deadlock detection: Added resource " << *resources[res_id] << '\n';
    }
    
    // Releases all thread resources and removes it, if it can fulfill all it's requests
    bool removeThread(const string& t_id){
        // if (!threads.at(t_id)->check_resources())
        //     throw runtime_error("Starvation detected\n");
        //if (threads.at(t_id)->canFulfillAllReq()){
            if (threads.erase(t_id))
                cout << "Deadlock detection: Removed thread " << t_id << endl;
            else
                cout << "Deadlock detection: could not remove thread " << t_id << endl;
            return true;
        //}
        //throw runtime_error("Thread " + t_id + "tried to exit in an unsafe state");
    }

    void removeResource(const string& res_id){
        resources.erase(res_id);
        cout << "Deadlock detection: Removed resources " << res_id << endl;
    }

    // Chooses what action to take based on the first instruction
    void parse(const vector<string>& instructions){
        if (instructions[0] == "CREATE")
            if (instructions[1] == "SEMAPHORE")
                parseCreate(instructions[1], instructions[2], instructions[3]);
            else
                parseCreate(instructions[1], instructions[2]);
        else if (instructions[0] == "DESTROY")
            parseDestroy(instructions[1], instructions[2]);
        else if (instructions[0] == "ALLOCATE")
            parseAllocate(instructions[1], instructions[2], instructions[3]);
        else if (instructions[0] == "REQUEST")
            parseRequest(instructions[1], instructions[2], instructions[3]);
        else if (instructions[0] == "RELEASE")
            parseRelease(instructions[1], instructions[2], instructions[3]);
    }
};

// Waits non-blockingly for children and returns the number of terminated children
int checkChildren(){
    int status;
    int term_child_count = 0;
    pid_t pid = waitpid(-1, &status, WNOHANG);

    while (pid > 0){
        cout << "Process " << pid << " terminated with status code: " << status << '\n';
        pid = waitpid(-1, &status, WNOHANG);
        term_child_count++;
    }
    return term_child_count;
}

int main(int argc, char* argv[]){
    string err_msg = validateInput(argc, argv);
    if (err_msg != ""){
        cerr << err_msg << '\n';
        return -1;
    }

    char* in_exe_name = argv[1];

    // Not using string in order to share aquire_log_lock and release_lock with sys_hook.c
    const char log_file_name[] = "log_file.txt";
    string parent_debug_log_name("p_debug_log.txt");
    string deadlk_check_file_name("deadlk_check_file.txt");

    // Open log file and clear past contents
    ofstream clear_file(log_file_name, ifstream::out|ifstream::trunc);
    clear_file.close();
    
    // Open the file again in read mode
    ifstream fin(log_file_name);
    if (!fin){
        cerr << "[ERROR]: Could not open log file\n";
        return -1;
    }
    off_t file_size = 0;
    std::streampos fin_pos = 0;

    // Everything that gets read is also printed to this file
    ofstream fout(parent_debug_log_name);

    // Creating child process for the deadlock suspect
    int rem_child_count = 1;
    pid_t pid = fork();

    //Child writes
    if (pid == 0){
        cout << "Child is executing...\n";
        char* args[] = {in_exe_name, NULL};
        // Using the hooked functions
        char* env[] = {"LD_PRELOAD=./sys_hook.so", NULL};
        int err = execve(in_exe_name, args, env);
        if (err < 0){
            perror("Child execve");
            return -1;
        }
    }
    else{ //Parent reads
        cout << "Parent is executing...\n";
        cout << "Child pid is " << pid << '\n';
        DetectionAlgo deadlock_det(deadlk_check_file_name);
        // Execute until all children finished execution
        while(rem_child_count > 0){
            // Checking for terminated children
            rem_child_count -= checkChildren();

            // Checking if there was actually something appended to the log file
            off_t curr_file_size = getFileSize(log_file_name);
            if (curr_file_size > file_size){
                cout << "File changed from " << file_size << " to " 
                     << curr_file_size << endl;
                file_size = curr_file_size;

                // Waiting to aquire the lock
                while(!aquire_log_lock());

                cout << "Parent is reading\n";

                // Restore read position
                fin.seekg(fin_pos);

                // Read the instruction string
                while (!fin.eof()){
                    string instr;
                    getline(fin, instr);

                    if (!instr.empty()){
                        // Print instruction to debug log
                        fout << instr << endl;
                        vector<string> split_instr = split(instr);
                        try{
                            deadlock_det.parse(split_instr);
                        } catch(runtime_error& e){
                            cout << e.what() << '\n';
                            cout << "Terminating process: " << pid << '\n';
                            kill(pid, SIGKILL);
                        }
                    }
                }
                // Clearing the eof bit
                fin.clear();
                // Keep track of the last read positon
                fin_pos = fin.tellg();

                release_lock();
            }
        }
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
    cout << "Deadlock Detection: Lock aquired\n";
    close(fd);
    return 1;
}

int release_lock(){
    int err = unlink(lock_file_name);
    if (err){
        perror("Deadlock Detection: Release lock unlink");
        exit(-1);
    }
    cout << "Deadlock Detection: Released lock\n";
    return 0;
}

// Splits by space the input string into a vector of strings
vector<string> split(const string& input_s){
    vector<string> split_string;
    stringstream ss(input_s);
    string curr_string;

    while (ss >> curr_string)
        split_string.push_back(curr_string);
    
    return split_string;
}

off_t getFileSize(const string& file_name){
    struct stat file_stat;
    if (stat(file_name.c_str(), &file_stat) == 0){
        return file_stat.st_size;
    }
    return 0;
}

bool compByNrReq(const DetectionThread& t1, const DetectionThread& t2){
    return t1.requested_res.size() < t2.requested_res.size();
}

string toString(ResourceTypes res_type){
    switch (res_type){
        case MUTEX:
            return "MUTEX";
        case SEMAPHORE:
            return "SEMAPHORE";
        default:
            return "";
    }
}

string toString(MessageTypes msg_type){
    switch (msg_type){
        case OK:
            return "OK";
        case WARNING:
            return "WARNING";
        case ERROR:
            return "ERROR";
        case DEBUG:
            return "DEBUG";
        default:
            return "";
    }
}

bool fileExists(const string& file_name){
    ifstream fin(file_name);

    return fin.good();
}

// Looks at the file header and checks whether it is an ELF executable
bool isElfExecutable(const char* filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) return false;

    unsigned char magic[EI_NIDENT];
    file.read(reinterpret_cast<char*>(magic), EI_NIDENT);
    return file.gcount() == EI_NIDENT &&
           magic[0] == ELFMAG0 &&
           magic[1] == ELFMAG1 &&
           magic[2] == ELFMAG2 &&
           magic[3] == ELFMAG3;
}

// Checks for user input and returns an error string if an error occurs, otherwise "" is returned
string validateInput(int argc, char* argv[]){
    if (argc != 2)
        return "[ERROR]: Invalid argument count. Correct usage ./lock_check ./[executable name]";
    
    char* in_exe_name = argv[1];
    // Checking if the user provided a regular file
    struct stat stat_buff;
    if (stat(in_exe_name, &stat_buff) != 0)
        return "[ERROR]: Trying to stat the file gives: " + (string)strerror(errno);
    
    if (!S_ISREG(stat_buff.st_mode))
        return "[ERROR]: Inputed file is not a regular executable file";

    // Check that the process has executing permission on the file
    int err = access(in_exe_name, X_OK);
    if (err < 0)
        return "[ERROR]: Process does not have executing rights on the inputed file";
    
    if (!isElfExecutable(in_exe_name))
        return "[ERROR]: Inputed file is not an ELF executable";
    
    return "";
}

ostream& operator<<(ostream& out, const DetectionResource& res){
    out << res.id << " " << toString(res.res_type) << " " << res.count << "/" << res.max_count; 
    return out;
}