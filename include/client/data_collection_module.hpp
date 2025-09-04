#pragma once
#include <string>
#include <vector>
#include "events/events.hpp"

// Abstract interface for data collection modules
class DataCollectionModule {
public:
    virtual ~DataCollectionModule() = default;
    
    // Called when client receives an event and needs to provide data
    // The event contains computation_metadata that can guide data collection
    virtual std::string collectData(const Event& event) = 0;
    
    // Split data into cryptographically secure shards for secret sharing
    // Returns vector of shards where sum of all shards = original value
    // Shards should appear random to prevent trivial value reconstruction
    virtual std::vector<std::string> shardData(const std::string& data, int num_shards) = 0;
    
    // Optional: called when all peer data is received for final aggregation
    virtual std::string aggregateData(const Event& event, 
                                    const std::vector<std::string>& peer_data) {
        return ""; // Default: no aggregation
    }
};