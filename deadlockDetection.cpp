#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <unordered_map>
#include <stack>
#include <set>

class DeadlockDetector {
public:
    DeadlockDetector() {}

    void detectDeadlock(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Eroare la deschiderea fisierului." << std::endl;
            return;
        }

        std::string line;
        std::unordered_map<std::string, std::set<std::string>> lockGraph;
        std::unordered_map<std::string, bool> mutexLocked;
        std::unordered_map<std::string, std::stack<std::string>> threadLocks;

        std::regex mutexRegex(R"(pthread_mutex_t\s+(\w+);)");
        std::regex lockCallRegex(R"(pthread_mutex_lock\(\s*&(\w+)\s*\))");
        std::regex unlockCallRegex(R"(pthread_mutex_unlock\(\s*&(\w+)\s*\))");

        while (std::getline(file, line)) {
            std::smatch match;

            if (std::regex_search(line, match, mutexRegex)) {
                std::string mutexName = match[1];
                mutexLocked[mutexName] = false;
            }


            if (std::regex_search(line, match, lockCallRegex)) {
                std::string mutexName = match[1];
                std::string threadName = "Thread_" + std::to_string(threadId);

                if (mutexLocked[mutexName]) {
                    lockGraph[mutexName].insert(threadName);
                } else {
                    mutexLocked[mutexName] = true;
                    threadLocks[threadName].push(mutexName);
                }
            }

            if (std::regex_search(line, match, unlockCallRegex)) {
                std::string mutexName = match[1];
                std::string threadName = "Thread_" + std::to_string(threadId);

                if (!threadLocks[threadName].empty() && threadLocks[threadName].top() == mutexName) {
                    threadLocks[threadName].pop();
                    mutexLocked[mutexName] = false;
                }
            }
        }

        if (hasCycle(lockGraph)) {
            std::cout << "Posibilitate de deadlock detectata!" << std::endl;
        } else {
            std::cout << "Nu s-a detectat deadlock." << std::endl;
        }

        file.close();
    }

private:
    bool hasCycle(const std::unordered_map<std::string, std::set<std::string>>& graph) {
        std::unordered_map<std::string, bool> visited;
        std::unordered_map<std::string, bool> inStack;

        for (const auto& node : graph) {
            if (!visited[node.first]) {
                if (dfs(node.first, graph, visited, inStack)) {
                    return true;
                }
            }
        }

        return false;
    }

    bool dfs(const std::string& node, const std::unordered_map<std::string, std::set<std::string>>& graph,
             std::unordered_map<std::string, bool>& visited, std::unordered_map<std::string, bool>& inStack) {
        visited[node] = true;
        inStack[node] = true;

        for (const auto& neighbor : graph.at(node)) {
            if (!visited[neighbor] && dfs(neighbor, graph, visited, inStack)) {
                return true;
            } else if (inStack[neighbor]) {
                return true;
            }
        }

        inStack[node] = false;
        return false;
    }

    int threadId = 0;
};

int main() {
    DeadlockDetector detector;
    detector.detectDeadlock("exemplu.C");
    return 0;
}