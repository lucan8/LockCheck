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
    unordered_map<string, shared_det_res>allocated_res;
    unordered_map<string, shared_det_res>requested_res;
    unordered_map<string, pair<shared_det_res,int>> res_counter;
public:
    DetectionThread(const string& s_id): AbstractObj(s_id){}

    const unordered_map<string, shared_det_res>& getAllocatedRes() const {return allocated_res;}
    const unordered_map<string, shared_det_res>& getRequestedRes() const {return requested_res;}
    
    // If the request can be completed, insert into allocated,
    // Otherwise in requested
    void request(const string& res_id, const shared_det_res& res){
        if(res_counter.find(res_id) == res_counter.end()){
            res_counter[res_id] = make_pair(res, -1);
        }
        else
        {
            res_counter[res_id].second -= 1;
        }
        if (res->decreaseCount()){
            allocated_res.insert(make_pair(res_id, res));
        }
        else
            requested_res.insert(make_pair(res_id, res));
    }

    //TO DO: Check if it actually exists
    void release(const string& res_id){
        allocated_res.erase(res_id);
        res_counter[res_id].second += 1;
    }
    bool check_resources()
        {
            for (auto counter : res_counter)
            {
                if(counter.second.second != 0){
                    return 0;
                }
            }
            return 1;
        }
};

class DetectionAlgo{
protected:
    unordered_map<string, DetectionThread> threads;
    unordered_map<string, DetectionResource> resources;
public:
    //TODO: One for starvation, one for deadlocks
    bool detect(){}

    //TODO: Should be used together with parse when CREATE instr appears
    void addThread(const string& s_id){
        DetectionThread* dt = new DetectionThread(s_id);
        threads.insert(make_pair(s_id, *dt));
    }
    void addResource(const string& s_id, ResourceTypes res_type){
        DetectionResource* dr = new DetectionResource(s_id, res_type);
        resources.insert(make_pair(s_id, *dr));
    }
    //TODO: Should be used together with parse when DESTROY instr appears
    void removeThread(const string& s_id){
        threads.erase(s_id);
    }
    void removeResource(const string& s_id){
        resources.erase(s_id);
    }

    void parse(const vector<string>& instructions){}

    void parseCreate(const string& obj_type, const string& obj_id){
        if (obj_type == "THREAD")
            addThread(obj_id);
        else if (obj_type == "MUTEX")
            addResource(obj_id, ResourceTypes::MUTEX);
        else if (obj_type == "SEMAPHORE")
            addResource(obj_id, ResourceTypes::SEMAPHORE);
    }

    void parseDestroy(const string& obj_type, const string& obj_id){
        if (obj_type == "THREAD")
            removeThread(obj_id);
        else if (obj_type == "MUTEX" || obj_type == "SEMAPHORE")
            removeResource(obj_id);
    }

    void parseAllocate(const string& thread_id, const string& res_type,
                       const string& res_id){
        
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
