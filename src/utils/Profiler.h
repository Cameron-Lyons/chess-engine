#pragma once

#include <chrono>
#include <unordered_map>
#include <string>
#include <atomic>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <mutex>

class Profiler {
public:
    struct ProfileData {
        uint64_t totalTime{0};
        uint64_t callCount{0};
        uint64_t nodes{0};
        
        double getAverageTime() const {
            return callCount > 0 ? static_cast<double>(totalTime) / callCount : 0;
        }
        
        double getNPS() const {
            return totalTime > 0 ? (nodes * 1000000000.0) / totalTime : 0;
        }
    };
    
private:
    std::unordered_map<std::string, ProfileData> profiles;
    mutable std::mutex profileMutex;
    std::chrono::high_resolution_clock::time_point startTime;
    
    Profiler() = default;
    
public:
    static Profiler& getInstance() {
        static Profiler instance;
        return instance;
    }
    
    class Timer {
        std::chrono::high_resolution_clock::time_point start;
        std::string name;
        uint64_t* nodeCounter;
        
    public:
        Timer(const std::string& n, uint64_t* nodes = nullptr) 
            : start(std::chrono::high_resolution_clock::now()), name(n), nodeCounter(nodes) {}
        
        ~Timer() {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            
            auto& profiler = Profiler::getInstance();
            std::lock_guard<std::mutex> lock(profiler.profileMutex);
            
            profiler.profiles[name].totalTime += duration;
            profiler.profiles[name].callCount++;
            if (nodeCounter) {
                profiler.profiles[name].nodes += *nodeCounter;
            }
        }
    };
    
    void reset() {
        std::lock_guard<std::mutex> lock(profileMutex);
        profiles.clear();
        startTime = std::chrono::high_resolution_clock::now();
    }
    
    void printReport() const {
        std::lock_guard<std::mutex> lock(profileMutex);
        
        if (profiles.empty()) {
            std::cout << "No profiling data available.\n";
            return;
        }
        
        auto totalTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now() - startTime).count();
        
        std::cout << "\n========== Performance Profile ==========\n";
        std::cout << "Total runtime: " << totalTime << " ms\n\n";
        
        std::vector<std::pair<std::string, ProfileData>> sortedProfiles(profiles.begin(), profiles.end());
        std::sort(sortedProfiles.begin(), sortedProfiles.end(),
                  [](const auto& a, const auto& b) { return a.second.totalTime > b.second.totalTime; });
        
        std::cout << std::left << std::setw(30) << "Function"
                  << std::right << std::setw(10) << "Calls"
                  << std::setw(12) << "Total (ms)"
                  << std::setw(12) << "Avg (µs)"
                  << std::setw(15) << "Nodes"
                  << std::setw(15) << "NPS (M/s)"
                  << std::setw(8) << "%"
                  << "\n";
        std::cout << std::string(102, '-') << "\n";
        
        for (const auto& [name, data] : sortedProfiles) {
            double totalMs = data.totalTime / 1000000.0;
            double avgUs = data.getAverageTime() / 1000.0;
            double nps = data.getNPS() / 1000000.0;
            double percentage = totalTime > 0 ? (totalMs / totalTime) * 100 : 0;
            
            std::cout << std::left << std::setw(30) << name
                      << std::right << std::setw(10) << data.callCount
                      << std::setw(12) << std::fixed << std::setprecision(2) << totalMs
                      << std::setw(12) << std::fixed << std::setprecision(2) << avgUs
                      << std::setw(15) << data.nodes
                      << std::setw(15) << std::fixed << std::setprecision(2) << nps
                      << std::setw(7) << std::fixed << std::setprecision(1) << percentage << "%"
                      << "\n";
        }
        std::cout << "=========================================\n";
    }
    
    uint64_t getNodeCount(const std::string& name) const {
        std::lock_guard<std::mutex> lock(profileMutex);
        auto it = profiles.find(name);
        return it != profiles.end() ? it->second.nodes : 0;
    }
    
    double getNPS(const std::string& name) const {
        std::lock_guard<std::mutex> lock(profileMutex);
        auto it = profiles.find(name);
        return it != profiles.end() ? it->second.getNPS() : 0;
    }
};

#ifdef ENABLE_PROFILING
    #define PROFILE_SCOPE(name) Profiler::Timer _timer(name)
    #define PROFILE_SCOPE_NODES(name, nodes) Profiler::Timer _timer(name, nodes)
    #define PROFILE_RESET() Profiler::getInstance().reset()
    #define PROFILE_REPORT() Profiler::getInstance().printReport()
#else
    #define PROFILE_SCOPE(name)
    #define PROFILE_SCOPE_NODES(name, nodes)
    #define PROFILE_RESET()
    #define PROFILE_REPORT()
#endif