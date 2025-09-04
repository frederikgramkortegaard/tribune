#pragma once
#include "client/data_collection_module.hpp"

// Mock implementation for development/testing
class MockDataCollectionModule : public DataCollectionModule {
public:
    MockDataCollectionModule(const std::string& client_id) : client_id_(client_id) {}
    
    std::string collectData(const Event& event) override;
    std::vector<std::string> shardData(const std::string& data, int num_shards) override;
    
private:
    std::string client_id_;
};