#include "client/data_collection_module.hpp"
#include "utils/logging.hpp"
#include <random>
#include <sstream>
#include <iostream>

std::string MockDataCollectionModule::collectData(const Event& event) {
    if (event.type_ == EventType::DataSubmission) {
        // Generate predictable test values based on client ID hash
        // This ensures each client always generates the same value for verification
        std::hash<std::string> hasher;
        size_t client_hash = hasher(client_id_);
        
        // Use hash to generate a value between 10-50 for easier math verification
        int predictable_value = 10 + (client_hash % 41); // 10-50 range
        
        std::stringstream ss;
        ss << "{";
        ss << "\"client_id\":\"" << client_id_ << "\",";
        ss << "\"event_type\":\"" << event.type_ << "\",";
        ss << "\"value\":" << predictable_value << ",";
        ss << "\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        ss << "}";
        
        DEBUG_DEBUG("Client " << client_id_ << " generated predictable value: " << predictable_value);
        
        return ss.str();
    }
    
    return "{\"error\":\"Unknown event type\"}";
}