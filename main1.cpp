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
        cout << "Thread " << this->id << " released " << res<< '\n';

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


    //Releases all alocated resources
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
        cout << "   Cloning resources...\n";
        // Clone resources and restart their count for reallocation
        for (const auto& r: resources){
            const string& res_id = r.first;
            const shared_det_res& res = r.second;
            clone_algo.addResource(res_id, res->getResType(), res->getMaxCount());
            cout << "       Cloned " << *clone_algo.resources[res_id] << "\n";
        }
        cout << "   Cloned " << resources.size() << " resources\n";
    }

    void cloneThreads(DetectionAlgo& clone_algo){
        cout << "   Cloning threads...\n";
        for (const auto& t: threads){
            // Create thread with identical id
            const string& t_id = t.first;
            const shared_det_thread& thread = t.second;
            clone_algo.addThread(t_id);

            // Get the just cloned thread and clone it's allocations and requests
            shared_det_thread& clone_thread = clone_algo.threads[t_id];

            cout << "   Thread " << t_id << ":\n";
            cloneThreadAllocations(clone_thread, thread->getAllocatedRes(), clone_algo);
            cloneThreadRequests(clone_thread, thread->getRequestedRes(), clone_algo);
        }
        cout << "   Cloned " << threads.size() << " threads\n";
    }

    void cloneThreadAllocations(shared_det_thread& clone_thread,
                                const unordered_map<string, shared_det_res>& cloned_res,
                                DetectionAlgo& clone_algo){
        
        // Going through the thread's allocated resources
        // And referencing their clones
        cout << "       Cloning thread allocations...\n";
        for (const auto& t_r: cloned_res){
            cout << "               Resource: " << *(t_r.second) << '\n';
            
            const string& t_res_id = t_r.first;
            const shared_det_res& t_clone_res = clone_algo.resources.at(t_res_id);
        
            clone_thread->allocate(t_res_id, t_clone_res);
        }
        cout << "       Cloned " << cloned_res.size() << " allocations\n";
    }

    void cloneThreadRequests(shared_det_thread& clone_thread,
                             const unordered_map<string, shared_det_res>& cloned_res,
                             DetectionAlgo& clone_algo){
        //Going through the thread's requested resources
        // and referencing their clones
        cout << "       Cloning Thread requests...\n";
        for (const auto& t_r: cloned_res){
            cout << "               Resource: " << *(t_r.second) << '\n';
            const string& t_res_id = t_r.first;
            const shared_det_res& t_clone_res = clone_algo.resources.at(t_res_id);
            clone_thread->request(t_res_id, t_clone_res);
        }
        cout << "       Cloned " << cloned_res.size() << " requests\n";
    }

public:
    bool detect(){}

    DetectionAlgo clone(){
        cout << "Cloning deadlock algo...\n";
        DetectionAlgo clone_algo;

        cloneResources(clone_algo);
        cloneThreads(clone_algo);

        return clone_algo;
    }

    // Frees safe threads until either there is only one thread remaining or no safe threads exist
    bool isSafeState(){
        // Getting current state of the algorithm
        DetectionAlgo clone_algo = clone();
        
        cout << "isSafeState: \n";
        int i = 0;

        // An iteration is safe only if there exists at least one thread that can fulfill all it's requests
        bool is_safe = true;
        
        while (clone_algo.threads.size() > 1 && is_safe){
            cout << "   Iteration " << i << '\n';

            is_safe = false;
            // Removing the threads that can fullfill all their requests and freeing their resources
            for (auto it = clone_algo.threads.begin(); it != clone_algo.threads.end();)
                if (it->second->canFulfillAllReq()){
                    cout << "       Removing thread " << it->first << '\n';
                    is_safe = true;
                    it->second->allocateFromRequested();
                    clone_algo.threads.erase(it++);
                }
                else
                    it++;
            
            cout << "       Remaining threads: " << clone_algo.threads.size() << '\n';
            cout << "   Iteration " << i << " finished\n";
            i++;
        }
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

int main(){
    const char log_file_name[] = "log_file.txt";
    const char parent_debug_log_name[] = "p_debug_log.txt";
    // Open log file and clear past contents
    ofstream clear_file(log_file_name, ifstream::out|ifstream::trunc);
    clear_file.close();
    
    // Open the file again in read mode
    ifstream fin(log_file_name);
    off_t file_size = 0;
    std::streampos fin_pos = 0;
    ofstream fout(parent_debug_log_name);

    int rem_child_count = 1;
    pid_t pid = fork();

    //Child writes
    if (pid == 0){
        cout << "Child is executing...\n";
        char* args[] = {"test_complex1", NULL};
        // Using the hooked functions
        char* env[] = {"LD_PRELOAD=./sys_hook.so", NULL};
        int err = execve("test_complex1", args, env);
        if (err < 0){
            perror("Child execve");
            return -1;
        }
    }
    else{ //Parent reads
        cout << "Parent is executing...\n";
        cout << "Child pid is " << pid << '\n';
        DetectionAlgo deadlock_det;
        // Execute until all children finished execution
        while(rem_child_count > 0){
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
                
                // Checking for terminated children
                // Done before releasing lock to avoid them writing to the file and terminating exactly before checking,
                // As the changes would not be seen by the parent
                rem_child_count -= checkChildren();

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

ostream& operator<<(ostream& out, const DetectionResource& res){
    out << res.id << " " << toString(res.res_type) << " " << res.count << "/" << res.max_count; 
    return out;
}