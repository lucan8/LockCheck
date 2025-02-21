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
using namespace std;

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
// Found bug on DetectionThread::release() : can't iterate over a container and remove from it at the same time

//TODO: Write unsafe thread exception
// When debugging I found there can be threads waiting for a resource without it being allocated to anyone
// I also found that this algorithm can detect both deadlocks and starvation
// But further thinking is needed to differentiate between the two
// TODO: Make abstract class Object that has an id
// TODO: We don't actually need to reallocate from requested before
// freeing a safe thread, but we need to modify the destructor
// to check for requested
// class MyException : public exception{
// private:
//     string msg;
//     string func_name;
// public:
//     virtual const char* what() const noexcept {
//         return ("[ERROR]: In " + string(__FILENAME__) + ", function " + func_name + " , line " + to_string(__LINE__) + ": " + msg).c_str();
//     }
// }
//class fullResourceException;
static const char lock_file_name[] = "lock_file.lock";
class DetectionResource;
class DetectionThread;
vector<string> split(const string& input_file);
off_t getFileSize(const string& file_name);

//Tries to create and open file
//On error displays error and exits(-1)
int aquire_log_lock();

//Unlinks lock file, returns 0 on succes, displays error and exit(-1) on failure 
int release_lock();
typedef shared_ptr<DetectionResource> shared_det_res;
typedef unique_ptr<DetectionThread> unique_det_thread;
typedef shared_ptr<DetectionThread> shared_det_thread;
enum ResourceTypes{
    MUTEX = 0,
    SEMAPHORE = 1,
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

    // Increases count only until max_count, returns true if successfull,
    // False otherwise
    bool increaseCount() {
        if (count >= max_count)
            return false;

        count++;
        return true;
    }

    // Decreases count only until 0, returns true if successfull, false otherwise
    bool decreaseCount(){
        if (isLocked())
            return false;
        
        count--;
        return true;
    }

    bool isLocked(){return count <= 0;}

    friend ostream& operator<<(ostream& out, const DetectionResource& res);
};

class DetectionThread{
private:
    string id;
    unordered_map<string, shared_det_res>allocated_res;
    unordered_map<string, shared_det_res>requested_res;
    unordered_map<string, pair<shared_det_res, int>> res_counter;

    // Tries to allocate the resource, returns true if successfull, false otherwise
    bool _allocate(const string& res_id,  shared_det_res res){
        bool successfull = res->decreaseCount();
        if (successfull)
            allocated_res.insert(make_pair(res_id, move(res)));
        return successfull;
    }

    // Move resource from requested to allocated
    bool _allocateFromRequested(const string& res_id){
        // Get the resource by id
        auto found_res = requested_res.find(res_id);
        
        if (found_res != requested_res.end()){
            // Trying to allocate the requested resource
            bool succesfull = this->_allocate(found_res->first, found_res->second);
            
            // If successfull, erase the resource from the requested container
            if (succesfull)
                requested_res.erase(res_id);
            return succesfull;   
        }
        return false;
    }

    // Helper function for the two public releases()
    // Increases the count on the resource an prints appropriate message to console
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

    bool allocate(const string& res_id, shared_det_res res){
        // allocateResCounter(res_id, res);
        // cout << "After allocate res count: " << res_counter[res_id].second << endl;
        
        // First try to allocate from requested
        bool successfull = this->_allocateFromRequested(res_id);

        // If that fails, allocate normally
        if (!successfull)
            successfull = this->_allocate(res_id, move(res));
        return successfull;
    }

    // Moves as much as possible from requested to allocated
    void allocateFromRequested(){
        for (const auto& res: this->requested_res)
            this->_allocateFromRequested(res.first);
    }

    void request(const string& res_id, shared_det_res res){
        requested_res.insert(make_pair(res_id, res));
    }
    
    
    // Releases resource with res_id
    void release(const string& res_id){
        // cout << "Small release " << res_id << '\n';
        // Remove and store the resource
        auto found_res = allocated_res.extract(res_id);

        if (!found_res.empty())
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

    // True if there is no locked resource in the requested container
    // False otherwise
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
            cerr << "Allocate: " << res_type << " Thread not found: " 
                 << thread_id << endl;
        }

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
            if (!clone_thread->allocate(t_res_id, t_clone_res))
                cout << "Allocate failed\n";
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

    // Returns a vector of threads' ids that can fulfill all their requests
    vector<string> getSafeThreads(){
        vector<string> safe_t_ids;
        safe_t_ids.reserve(threads.size());

        for (auto& t: threads)
            if (t.second->canFulfillAllReq())
                safe_t_ids.push_back(t.first);
        
        return safe_t_ids;
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

    bool isSafeState(){
        // Getting current state of the algorithm
        DetectionAlgo clone_algo = clone();
        
        cout << "isSafeState: \n";
        int i = 0;

        while (clone_algo.threads.size() > 1){
            cout << "   Iteration " << i << '\n';

            vector<string> safe_threads = clone_algo.getSafeThreads();
            cout << "       Found " << safe_threads.size() << " safe threads\n";

            // If no threads can exit, we found a deadlock
            if (safe_threads.empty()){
                cout << "       No safe threads found\n" << "       Remaining Threads: " << clone_algo.threads.size() << '\n';
                return false;
            }
            
            cout << "       Reallocating resources for safe threads\n";
            // Fulfill the safe thread's requests
            for (auto& t_id: safe_threads)
                clone_algo.threads[t_id]->allocateFromRequested();
            
            cout << "       Removing safe threads\n";
            // Removing each thread that can exit and freeing their resources
            for (const auto& t_id: safe_threads)
                clone_algo.removeThread(t_id);
            i++;
        }
        return true;
    }

    void addThread(const string& t_id){
        threads.insert(make_pair(t_id, make_shared<DetectionThread>(t_id)));

        // if (threads.at(t_id)->check_resources())
        cout << "Deadlock detection: Created thread " << t_id << endl;
    }

    void addResource(const string& res_id, ResourceTypes res_type, unsigned int count){
        resources.insert(make_pair(res_id,
                                   make_shared<DetectionResource>(res_id, res_type, count)));
        
        cout << "Deadlock detection: Added resource " << res_id << endl;
    }
    
    // Releases all thread resources and removes it, if it can fulfill all it's requests
    bool removeThread(const string& t_id){
        // if (!threads.at(t_id)->check_resources())
        //     throw runtime_error("Starvation detected\n");
        if (threads.at(t_id)->canFulfillAllReq()){
            if (threads.erase(t_id))
                cout << "Deadlock detection: Removed thread " << t_id << endl;
            else
                cout << "Deadlock detection: could not remove thread " << t_id << endl;
            return true;
        }
        throw runtime_error("Thread " + t_id + "tried to exit in an unsafe state");
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
        DetectionAlgo deadlock_det;
        while(true){
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
                            cout << e.what();
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
    cout << "Released lock\n";
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

ostream& operator<<(ostream& out, const DetectionResource& res){
    out << res.id << " ";
    if (res.res_type == ResourceTypes :: MUTEX)
        out << "MUTEX " << res.count;
    else
        out << "SEMAPHORE " << res.count << "/" << res.max_count; 
    return out;
}