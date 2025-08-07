#include <iostream>
#include <vector>
#include <mutex>
#include <chrono>
#include <iomanip>

// Global execution tracker
class ExecutionTracker {
private:
    static std::mutex mutex_;
    static std::vector<std::string> events_;
    static auto startTime_;
    
public:
    static void log(const std::string& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime_).count();
        
        std::cout << "[" << std::setw(8) << elapsed << "μs] " << event << std::endl;
        events_.push_back(event);
    }
    
    static void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        events_.clear();
        startTime_ = std::chrono::high_resolution_clock::now();
    }
    
    static void printSummary() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "\n=== EXECUTION ORDER SUMMARY ===" << std::endl;
        for (size_t i = 0; i < events_.size(); i++) {
            std::cout << i+1 << ". " << events_[i] << std::endl;
        }
    }
};

// Initialize static members
std::mutex ExecutionTracker::mutex_;
std::vector<std::string> ExecutionTracker::events_;
auto ExecutionTracker::startTime_ = std::chrono::high_resolution_clock::now();

// Macros for easy tracking
#define TRACK_EXECUTION(msg) ExecutionTracker::log(msg)
#define TRACK_FUNCTION() ExecutionTracker::log(std::string(__FUNCTION__) + " called")
#define TRACK_STAGE(stage, msg) ExecutionTracker::log("[" + std::string(stage) + "] " + msg)

/* Usage in pipeline:
 
At start of main():
    ExecutionTracker::reset();
    TRACK_STAGE("INIT", "Application starting");

In updateInstanceBuffer():
    TRACK_STAGE("UPDATE", "Instance buffer update started");
    TRACK_STAGE("UPDATE", "Water instances: " + std::to_string(waterCount));

In createGraphicsPipeline():
    TRACK_STAGE("PIPELINE", "Creating pipeline");
    TRACK_STAGE("PIPELINE", "Attributes: " + std::to_string(attributeCount));

In render loop:
    TRACK_STAGE("RENDER", "Frame " + std::to_string(frameNum));

At shutdown:
    ExecutionTracker::printSummary();
*/