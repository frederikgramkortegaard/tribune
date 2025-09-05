#pragma once
#include "mpc/mpc_computation.hpp"
#include <vector>

class FederatedAggregationComputation : public MPCComputation {
public:
    FederatedAggregationComputation(double learning_rate = 0.01);
    
    // Client-side: Sum received masked gradient shards
    // Returns masked contribution (masks still active)
    std::string compute(const std::vector<std::string>& shards, 
                       const nlohmann::json& metadata = nlohmann::json::object()) override;
                       
    // Server-side: Aggregate all client contributions and update model
    // This is where pairwise masks cancel out → true global gradient → updated weights
    std::string aggregateResults(const std::vector<std::string>& client_results) override;
    
    std::string getComputationType() const override { 
        return "federated_aggregation"; 
    }
    
    // Model weight management
    void setModelWeights(const std::vector<double>& weights);
    std::vector<double> getModelWeights() const;
    
private:
    std::vector<double> current_weights_;
    double learning_rate_;
    
    // Helper functions for gradient operations
    std::vector<double> deserializeGradient(const std::string& data);
    std::string serializeGradient(const std::vector<double>& gradient);
    
    // Model weight operations
    std::string serializeModelWeights(const std::vector<double>& weights);
    std::vector<double> deserializeModelWeights(const std::string& data);
    std::vector<double> applyGradientToWeights(const std::vector<double>& gradient);
    
    // Element-wise addition of gradient vectors
    std::vector<double> addGradients(const std::vector<double>& a, const std::vector<double>& b);
};