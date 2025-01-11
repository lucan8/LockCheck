#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <unordered_map>

void detectStarvation(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Eroare la deschiderea fisierului." << std::endl;
        return;
    }

    std::string line;
    std::unordered_map<std::string, int> mutexUsage; 
    std::regex lockRegex(R"(std::mutex\s+(\w+);)"); 
    std::regex lockCallRegex(R"((\w+)\.lock\(\))");
    std::regex unlockCallRegex(R"((\w+)\.unlock\(\))");  

    while (std::getline(file, line)) {
        std::smatch match;
        
        if (std::regex_search(line, match, lockRegex)) {
            std::string mutexName = match[1];
            mutexUsage[mutexName] = 0;
        }

        if (std::regex_search(line, match, lockCallRegex)) {
            std::string mutexName = match[1];
            if (mutexUsage.find(mutexName) != mutexUsage.end()) {
                mutexUsage[mutexName]++;
            }
        }

        if (std::regex_search(line, match, unlockCallRegex)) {
            std::string mutexName = match[1];
            if (mutexUsage.find(mutexName) != mutexUsage.end()) {
                mutexUsage[mutexName]--;
            }
        }
    }

    file.close();

    std::cout << "Raport de utilizare a mutex-urilor:\n";
    for (const auto& entry : mutexUsage) {
        std::cout << "Mutex: " << entry.first << " | Blocari active: " << entry.second << std::endl;
        if (entry.second > 0) {
            std::cout << "Posibilitate de starvation detectata pentru mutex-ul " << entry.first << " (neeliberat corespunzator)." << std::endl;
        }
    }
}
