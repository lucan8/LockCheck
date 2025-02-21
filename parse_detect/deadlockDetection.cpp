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
        std::regex lockMutexRegex(R"(pthread_mutex_lock\(\s*&(\w+)\s*\))");
        std::regex unlockMutexRegex(R"(pthread_mutex_unlock\(\s*&(\w+)\s*\))");
        std::regex lockSemRegex(R"(sem_wait\(\s*&(\w+)\s*\))");
        std::regex unlockSemRegex(R"(sem_post\(\s*&(\w+)\s*\))");

        std::string currentThread = "main";
        while (std::getline(file, line)) {
            std::smatch match;

            if (std::regex_search(line, match, lockMutexRegex)) {
                std::string mutexName = match[1].str();
                handleLock(threadLocks, lockGraph, currentThread, mutexName);
            }
            if (std::regex_search(line, match, unlockMutexRegex)) {
                std::string mutexName = match[1].str();
                handleUnlock(threadLocks, currentThread, mutexName);
            }
            if (std::regex_search(line, match, lockSemRegex)) {
                std::string semName = match[1].str();
                handleLock(threadLocks, lockGraph, currentThread, semName);
            }
            if (std::regex_search(line, match, unlockSemRegex)) {
                std::string semName = match[1].str();
                handleUnlock(threadLocks, currentThread, semName);
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
    void handleLock(std::unordered_map<std::string, std::unordered_set<std::string>>& threadLocks,
                    std::unordered_map<std::string, std::unordered_set<std::string>>& lockGraph,
                    const std::string& currentThread,
                    const std::string& resourceName) {
        if (!threadLocks[currentThread].empty()) {
            for (const auto& heldResource : threadLocks[currentThread]) {
                lockGraph[heldResource].insert(resourceName);
            }
        }
        threadLocks[currentThread].insert(resourceName);
    }

    void handleUnlock(std::unordered_map<std::string, std::unordered_set<std::string>>& threadLocks,
                      const std::string& currentThread,
                      const std::string& resourceName) {
        threadLocks[currentThread].erase(resourceName);
    }

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
