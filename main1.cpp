#include <iostream>
#include <vector>
#include <sstream>
#include <pthread.h>
#include <semaphore.h>
#include <unordered_set>
#include <unordered_map>
#include <memory>
using namespace std;

class DetectionResource;
class DetectionThread;
typedef shared_ptr<DetectionResource> shared_det_res;

enum ResourceTypes{
    MUTEX = 0,
    SEMAPHORE = 1,
};

class AbstractObj{
protected:
    string id;
public:
    // Set the id, increase id count and map the id stirng
    AbstractObj(const string& id): id(id){}

    const string& getId() const{ return id;}
};

class DetectionResource: public AbstractObj{
private:
    ResourceTypes res_type;
    size_t count;
public:
    DetectionResource(const string& s_id, const ResourceTypes& res_type,
                      size_t count = 1): 
        AbstractObj(s_id), res_type(res_type), count(count){
        
        //Mutexes are just semaphores with count of 1
        if (res_type == ResourceTypes::MUTEX)
            count = 1;
    }

    ResourceTypes getResType() const {return res_type; }
    size_t getCount() const {return count; }

    void increaseCount() {count++;}
    // Decreases count if count > 0, returns if decrease is successfull
    bool decreaseCount(){
        if (count == 0)
            return false;
        
        count--;
        return true;
    }
};

class DetectionThread: public AbstractObj{
private:
    unordered_map<string, shared_det_res>allocated_res;
    unordered_map<string, shared_det_res>requested_res;
public:
    DetectionThread(const string& s_id): AbstractObj(s_id){}

    const unordered_map<string, shared_det_res>& getAllocatedRes() const {return allocated_res;}
    const unordered_map<string, shared_det_res>& getRequestedRes() const {return requested_res;}
    
    void allocate(const string& res_id,  shared_det_res res){
        // First look to see if the resource has already been requested
        unordered_map<string, shared_det_res> :: iterator found_res_it
            = requested_res.find(res_id);
        
        // If not requested just add it to the allocated map
        if (found_res_it == requested_res.end())
            allocated_res.insert(make_pair(res_id, res));
        else{ //If already requested move the data from one map to the other
            allocated_res.insert(make_pair(move(found_res_it->first),
                                           move(found_res_it->second)));
            requested_res.erase(found_res_it);
        }
    }

    // If the request can be completed, insert into allocated,
    // Otherwise in requested
    void request(const string& res_id, shared_det_res res){
        requested_res.insert(make_pair(res_id, res));
    }

    //TO DO: Check if it actually exists
    void release(const string& res_id){
        allocated_res.erase(res_id);
    }
};

class DetectionAlgo{
protected:
    unordered_map<string, DetectionThread*> threads;
    unordered_map<string, shared_det_res> resources;

    // Helper functions for public "parse"
    void parseCreate(const string& obj_type, const string& res_count, 
                     const string& obj_id){
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

    void parseAllocate(const string& thread_id, const string& res_type,
                       const string& res_id){
        threads.at(thread_id)->allocate(res_id, resources.at(res_id));
    }

    void parseRequest(const string& thread_id, const string& res_type,
                       const string& res_id){
        threads.at(thread_id)->request(res_id, resources.at(res_id));
    }

    void parseRelease(const string& thread_id, const string& res_type,
                       const string& res_id){
        threads.at(thread_id)->release(res_id);
    }
public:
    //TODO: One for starvation, one for deadlocks
    bool detect(){}

    void addThread(const string& t_id){
        threads.insert(make_pair(t_id, new DetectionThread(t_id)));
    }

    void addResource(const string& res_id, ResourceTypes res_type, unsigned int count){
        resources.insert(make_pair(res_id,
                                   make_shared<DetectionResource>
                                   (new DetectionResource(res_id, res_type, count))));
    }
    
    void removeThread(const string& t_id){
        threads.erase(t_id);
    }

    void removeResource(const string& res_id){
        resources.erase(res_id);
    }

    // Chooses what action to take based on the first instruction
    void parse(const vector<string>& instructions){
        if (instructions[0] == "CREATE")
            parseCreate(instructions[1], instructions[2], instructions[3]);
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

}


// Splits by space the input string into a vector of strings
vector<string> split(const string& input_s){
    vector<string> split_string;
    stringstream ss;
    string curr_string;

    while (ss >> curr_string)
        split_string.push_back(curr_string);
    
    return split_string;
}