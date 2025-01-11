#include <iostream>
#include <vector>
#include <unordered_map>
#include <utility>
using namespace std;

class sem{
    static int id_gen;
    int id;
    int val;
public:
    sem() {
        val = 0;
        id = id_gen;
        id_gen++;
        }
    sem(int k) {
        val = k;
        id = id_gen;
        id_gen++;
    }
    void wait() {
        if(val > 0){
            val --;
        }
        else
        {
            val = 0;
        }
    }
    void post(){
        val++;
    }
    int get_id(){return id;}
    int get_val(){return val;}
};
int sem::id_gen = 0;
class proces{
    static int id_gen;
    int id;
public:
    unordered_map<int, pair<sem*,int>> sems;
    proces() {id = id_gen++;
              }
    bool finish()
        {
            for (auto i : sems)
            {
                if(i.second.second != 0){
                    return 0;
                }
            }
            return 1;
        }
};
int proces::id_gen = 0;
vector<sem*> semafoare;
vector<proces*> procese;
void sem_wait(proces* p, sem* s){
    s->wait();
    if(p->sems.find(s->get_id()) == p->sems.end()){
        p->sems[s->get_id()] = make_pair(s, -1);
    }
    else
    {
        p->sems[s->get_id()].second -= 1;
    }
}
void sem_post(proces* p, sem* s){
    s->post();
    if(p->sems.find(s->get_id()) == p->sems.end()){
        p->sems[s->get_id()] = make_pair(s, 1);
    }
    else
    {
        p->sems[s->get_id()].second += 1;
    }
}
int main()
{
    semafoare.resize(1000);
    procese.resize(1000);
    for(int i = 0; i < 10; i++){
        sem* s = new sem(5);
        semafoare[i] = s;
    }
    for(int i = 0; i < 10; i++){
        proces* p = new proces();
        procese[i] = p;
    }
    sem_wait(procese[0],semafoare[0]);
    sem_post(procese[0], semafoare[0]);
    cout << procese[0]->finish();
    return 0;
}
