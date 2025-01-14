#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <unordered_map>
#include <unordered_set>
class DeadlockDetector {
public:
    void detectDeadlock(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Eroare la deschiderea fișierului." << std::endl;
            return;
        }

        std::string line;
        std::unordered_map<std::string, std::unordered_set<std::string>> lockGraph;
        std::unordered_map<std::string, std::unordered_set<std::string>> threadLocks;
        std::regex lockRegex(R"(pthread_mutex_lock\(\s*&(\w+)\s*\))");
        std::regex unlockRegex(R"(pthread_mutex_unlock\(\s*&(\w+)\s*\))");

        std::string currentThread = "main";
        while (std::getline(file, line)) {
            std::smatch match;
            if (std::regex_search(line, match, lockRegex)) {
                std::string mutexName = match[1].str();
                if (!threadLocks[currentThread].empty()) {
                    for (const auto& heldMutex : threadLocks[currentThread]) {
                        lockGraph[heldMutex].insert(mutexName);
                    }
                }
                threadLocks[currentThread].insert(mutexName);
            }

            if (std::regex_search(line, match, unlockRegex)) {
                std::string mutexName = match[1].str();
                threadLocks[currentThread].erase(mutexName);
            }
        }

        file.close();

        if (hasCycle(lockGraph)) {
            std::cout << "Deadlock detectat!" << std::endl;
        } else {
            std::cout << "Nu s-a detectat deadlock." << std::endl;
        }
    }

private:
    bool hasCycle(const std::unordered_map<std::string, std::unordered_set<std::string>>& graph) {
        std::unordered_map<std::string, bool> visited;
        std::unordered_map<std::string, bool> inStack;

        std::function<bool(const std::string&)> dfs = [&](const std::string& node) {
            if (inStack[node]) return true;
            if (visited[node]) return false;

            visited[node] = true;
            inStack[node] = true;

            if (graph.find(node) != graph.end()) {
                for (const auto& neighbor : graph.at(node)) {
                    if (dfs(neighbor)) return true;
                }
            }

            inStack[node] = false;
            return false;
        };

        for (const auto& entry : graph) {
            if (!visited[entry.first]) {
                if (dfs(entry.first)) {
                    return true;
                }
            }
        }
        return false;
    }
};

int main() {
    DeadlockDetector detector;
    detector.detectDeadlock("deadlock.C");
    return 0;
}
