#pragma once
#include <string>
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

// Mock implementation for development/testing
class MockDataCollectionModule : public DataCollectionModule {
public:
    MockDataCollectionModule(const std::string& client_id) : client_id_(client_id) {}
    
    std::string collectData(const Event& event) override;
    
private:
    std::string client_id_;
};