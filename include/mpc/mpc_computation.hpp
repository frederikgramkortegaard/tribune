#pragma once
#include "events/events.hpp"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class MPCComputation {
public:
    virtual ~MPCComputation() = default;
    virtual std::string compute(const std::vector<std::string>& shards, 
                               const Event& event) = 0;
    virtual std::string aggregateResults(const std::vector<std::string>& client_results) = 0;
    virtual std::string getComputationType() const = 0;
};