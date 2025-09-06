#pragma once
#include "events/events.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class MPCComputation {
public:
    virtual ~MPCComputation() = default;
    
    // Client-side: Split raw data into cryptographically secure shards
    // Returns vector of shards where the number of shards = event.participants.size()
    // Each shard will be sent to a different peer (including keeping one for ourselves)
    virtual std::vector<std::string> shardData(const std::string& raw_data, 
                                              const Event& event) = 0;
    
    // Client-side: Compute local result from collected shards
    virtual std::string compute(const std::vector<std::string>& shards, 
                               const Event& event) = 0;
    
    // Server-side: Aggregate all client results into final output
    virtual std::string aggregateResults(const std::vector<std::string>& client_results,
                                        const Event& event) = 0;
    
    virtual std::string getComputationType() const = 0;
};