#include "../include/federated_computation.hpp"
#include "utils/logging.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>

FederatedAggregationComputation::FederatedAggregationComputation(double learning_rate) 
    : learning_rate_(learning_rate) {
    // Initialize with default weights for logout prediction: [bias, hour, day_of_week, hours_online]
    current_weights_ = {0.0, 0.0, 0.0, 0.0};
    DEBUG_INFO("Initialized FederatedAggregationComputation with learning rate: " << learning_rate_);
}

std::string FederatedAggregationComputation::compute(const std::vector<std::string>& shards, 
                                                    const nlohmann::json& metadata) {
    DEBUG_INFO("=== FEDERATED AGGREGATION COMPUTE ===");
    DEBUG_INFO("Processing " << shards.size() << " masked gradient shards from peers");
    
    if (shards.empty()) {
        DEBUG_WARN("No shards received for federated computation");
        return serializeGradient({});
    }
    
    // Reconstruct and sum all masked gradients
    std::vector<double> my_contribution;
    bool first_shard = true;
    
    for (size_t i = 0; i < shards.size(); ++i) {
        try {
            auto masked_gradient = deserializeGradient(shards[i]);
            if (masked_gradient.empty()) {
                DEBUG_WARN("Skipping empty gradient shard " << i);
                continue;
            }
            
            DEBUG_DEBUG("Shard " << i << " gradient size: " << masked_gradient.size());
            
            if (first_shard) {
                my_contribution = masked_gradient;
                first_shard = false;
            } else {
                // Add this masked gradient to our running sum
                my_contribution = addGradients(my_contribution, masked_gradient);
            }
            
        } catch (const std::exception& e) {
            DEBUG_ERROR("Failed to process shard " << i << ": " << e.what());
            continue;
        }
    }
    
    DEBUG_INFO("Computed contribution with " << my_contribution.size() << " parameters");
    DEBUG_DEBUG("Contribution sample: [" << (my_contribution.size() > 0 ? my_contribution[0] : 0.0) 
              << ", " << (my_contribution.size() > 1 ? my_contribution[1] : 0.0) << ", ...]");
    DEBUG_INFO("=====================================");
    
    // Return my masked contribution to server
    // Masks will cancel when server aggregates all client contributions
    return serializeGradient(my_contribution);
}

std::string FederatedAggregationComputation::aggregateResults(const std::vector<std::string>& client_results) {
    DEBUG_INFO("=== SERVER FEDERATED AGGREGATION ===");
    DEBUG_INFO("Aggregating results from " << client_results.size() << " clients");
    
    if (client_results.empty()) {
        DEBUG_WARN("No client results to aggregate");
        return serializeModelWeights({});
    }
    
    // Sum all client contributions - this is where masks cancel out!
    std::vector<double> global_gradient;
    bool first_result = true;
    
    for (size_t i = 0; i < client_results.size(); ++i) {
        try {
            auto client_contribution = deserializeGradient(client_results[i]);
            if (client_contribution.empty()) {
                DEBUG_WARN("Skipping empty client result " << i);
                continue;
            }
            
            DEBUG_DEBUG("Client " << i << " contribution size: " << client_contribution.size());
            
            if (first_result) {
                global_gradient = client_contribution;
                first_result = false;
            } else {
                // Add this client's contribution to global sum
                global_gradient = addGradients(global_gradient, client_contribution);
            }
            
        } catch (const std::exception& e) {
            DEBUG_ERROR("Failed to process client result " << i << ": " << e.what());
            continue;
        }
    }
    
    DEBUG_INFO("Global gradient computed with " << global_gradient.size() << " parameters");
    DEBUG_DEBUG("Global gradient sample: [" << (global_gradient.size() > 0 ? global_gradient[0] : 0.0) 
              << ", " << (global_gradient.size() > 1 ? global_gradient[1] : 0.0) << ", ...]");
    
    // Apply gradient to model weights with learning rate
    auto updated_weights = applyGradientToWeights(global_gradient);
    
    DEBUG_INFO("Updated model weights, returning for next round");
    DEBUG_INFO("===================================");
    return serializeModelWeights(updated_weights);
}

std::vector<double> FederatedAggregationComputation::deserializeGradient(const std::string& data) {
    try {
        nlohmann::json j = nlohmann::json::parse(data);
        return j.get<std::vector<double>>();
    } catch (const std::exception& e) {
        DEBUG_ERROR("Failed to deserialize gradient: " << e.what());
        return {};
    }
}

std::string FederatedAggregationComputation::serializeGradient(const std::vector<double>& gradient) {
    nlohmann::json j = gradient;
    return j.dump();
}

std::vector<double> FederatedAggregationComputation::addGradients(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    
    if (a.size() != b.size()) {
        DEBUG_ERROR("Gradient size mismatch: " << a.size() << " vs " << b.size());
        throw std::runtime_error("Cannot add gradients of different sizes");
    }
    
    std::vector<double> result(a.size());
    for (size_t i = 0; i < a.size(); ++i) {
        result[i] = a[i] + b[i];
    }
    
    return result;
}

void FederatedAggregationComputation::setModelWeights(const std::vector<double>& weights) {
    current_weights_ = weights;
    DEBUG_INFO("Updated model weights to size: " << weights.size());
}

std::vector<double> FederatedAggregationComputation::getModelWeights() const {
    return current_weights_;
}

std::string FederatedAggregationComputation::serializeModelWeights(const std::vector<double>& weights) {
    nlohmann::json j;
    j["model_weights"] = weights;
    j["learning_rate"] = learning_rate_;
    return j.dump();
}

std::vector<double> FederatedAggregationComputation::deserializeModelWeights(const std::string& data) {
    try {
        nlohmann::json j = nlohmann::json::parse(data);
        if (j.contains("model_weights")) {
            return j["model_weights"].get<std::vector<double>>();
        }
        return {};
    } catch (const std::exception& e) {
        DEBUG_ERROR("Failed to deserialize model weights: " << e.what());
        return {};
    }
}

std::vector<double> FederatedAggregationComputation::applyGradientToWeights(const std::vector<double>& gradient) {
    if (current_weights_.empty()) {
        DEBUG_WARN("No current weights set, initializing with gradient size");
        current_weights_ = std::vector<double>(gradient.size(), 0.0);
    }
    
    if (current_weights_.size() != gradient.size()) {
        DEBUG_ERROR("Weight/gradient size mismatch: " << current_weights_.size() << " vs " << gradient.size());
        throw std::runtime_error("Cannot apply gradient: size mismatch with weights");
    }
    
    // Apply gradient descent: weights = weights - learning_rate * gradient
    std::vector<double> updated_weights(current_weights_.size());
    for (size_t i = 0; i < current_weights_.size(); ++i) {
        updated_weights[i] = current_weights_[i] - learning_rate_ * gradient[i];
    }
    
    DEBUG_DEBUG("Applied gradient with learning rate " << learning_rate_);
    DEBUG_DEBUG("Weight update sample: " << current_weights_[0] << " -> " << updated_weights[0]);
    
    // Store the updated weights
    current_weights_ = updated_weights;
    
    return updated_weights;
}