#pragma once
#include <string>
#include <vector>
#include "events/events.hpp"

// Abstract interface for data collection modules
class DataCollectionModule {
public:
    virtual ~DataCollectionModule() = default;
    
    // Called when client receives an event and needs to provide data
    virtual std::string collectData(const Event& event) = 0;
    
    // Optional: called when all peer data is received for final aggregation
    virtual std::string aggregateData(const Event& event, 
                                    const std::vector<std::string>& peer_data) {
        return ""; // Default: no aggregation
    }
};