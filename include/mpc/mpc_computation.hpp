#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class MPCComputation {
public:
    virtual ~MPCComputation() = default;
    virtual std::string compute(const std::vector<std::string>& shards, 
                               const nlohmann::json& metadata = nlohmann::json::object()) = 0;
    virtual std::string aggregateResults(const std::vector<std::string>& client_results) = 0;
    virtual std::string getComputationType() const = 0;
};

class SumComputation : public MPCComputation {
public:
    std::string compute(const std::vector<std::string>& shards, 
                       const nlohmann::json& metadata = nlohmann::json::object()) override;
    std::string aggregateResults(const std::vector<std::string>& client_results) override;
    std::string getComputationType() const override { return "sum"; }
};