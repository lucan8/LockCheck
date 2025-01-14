#include <iostream>
#include <vector>
#include <sstream>
#include <pthread.h>
#include <semaphore.h>
#include <unordered_set>
#include <unordered_map>
#include <memory>
using namespace std;
typedef shared_ptr<DetectionResource> shared_det_res;

//TODO: SEE PARSE AND DEADLOCKALGO BELOW

enum ResourceTypes{
    MUTEX = 0,
    SEMAPHORE = 1,
};

class AbstractObj{
protected:
    static unordered_map<string, int> id_map;
    static size_t id_count;
    size_t id;
public:
    // Set the id, increase id count and map the id stirng
    AbstractObj(const string& s_id): id(id_count++){
        id_map[s_id] = this->id;
    }

    size_t getId() const{ return id;}
};

class DetectionResource: public AbstractObj{
private:
    ResourceTypes res_type;
    size_t count;
public:
    DetectionResource(const string& s_id, const ResourceTypes& res_type,
                      size_t count = 0): 
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
    unordered_map<size_t, shared_det_res>allocated_res;
    unordered_map<size_t, shared_det_res>requested_res;
public:
    DetectionThread(const string& s_id): AbstractObj(s_id){}

    const unordered_map<size_t, shared_det_res>& getAllocatedRes() const {return allocated_res;}
    const unordered_map<size_t, shared_det_res>& getRequestedRes() const {return requested_res;}
    
    // If the request can be completed, insert into allocated,
    // Otherwise in requested
    void request(size_t res_id, const shared_det_res& res){
        if (res->decreaseCount())
            allocated_res.insert(make_pair(res_id, res));
        else
            requested_res.insert(make_pair(res_id, res));
    }

    //TO DO: Check if it actually exists
    void release(size_t res_id){
        allocated_res.erase(res_id);
    }
};

class DetectionAlgo{
protected:
    vector<DetectionThread*> threads;
    vector<DetectionResource*> resources;
public:
    //TODO: One for starvation, one for deadlocks
    bool detect(){}

    //TODO: Should be used together with parse when CREATE instr appears
    void addThread();
    void addResource();
    //TODO: Should be used together with parse when DESTROY instr appears
    void removeThread();
    void removeResource();
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

// TODO:
// void parse(const vector<string>& instructions){
//     if (instructions[0] == "CREATE")
//         if (instructions[1] == "THREAD")
//             continue;
//         else if (instructions[1] == "MUTEX")
//             continue;
//         else if (instructions[1] == "SEMAPHORE")
//             continue;
//     else if (instructions[0] == "DESTROY")
//         if (instructions[1] == "THREAD")
//             continue;
//         else if (instructions[1] == "MUTEX")
//             continue;
//         else if (instructions[1] == "SEMAPHORE")
//             continue;
// }
