#include "client/data_collection_module.hpp"
#include <random>
#include <sstream>

std::string MockDataCollectionModule::collectData(const Event& event) {
    // Generate mock data based on event type and client
    std::random_device rd;
    std::mt19937 gen(rd());
    
    if (event.type_ == EventType::DataSubmission) {
        // Mock numeric data - could be sensor readings, ML features, etc.
        std::uniform_real_distribution<> dis(1.0, 100.0);
        
        std::stringstream ss;
        ss << "{";
        ss << "\"client_id\":\"" << client_id_ << "\",";
        ss << "\"event_type\":\"" << event.type_ << "\",";
        ss << "\"value\":" << dis(gen) << ",";
        ss << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        ss << "}";
        
        return ss.str();
    }
    
    return "{\"error\":\"Unknown event type\"}";
}