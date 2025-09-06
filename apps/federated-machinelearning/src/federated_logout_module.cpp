#include "../include/federated_logout_module.hpp"
#include "utils/logging.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <random>
#include <ctime>
#include <stdexcept>

FederatedLogoutGradientModule::FederatedLogoutGradientModule(const std::string& client_id, 
                                                           const std::string& private_key) 
    : client_id_(client_id), private_key_(private_key) {
    DEBUG_INFO("Created FederatedLogoutGradientModule for client: " << client_id_);
}

std::string FederatedLogoutGradientModule::collectData(const Event& event) {
    DEBUG_INFO("Collecting logout prediction data for event: " << event.event_id);
    
    // Get current features
    double hours_online = getHoursOnlineToday();
    int current_hour = getCurrentHour();
    int day_of_week = getCurrentDayOfWeek();
    
    // Create feature vector [bias=1.0, hour, day_of_week, hours_online]
    std::vector<double> features = {1.0, static_cast<double>(current_hour), 
                                   static_cast<double>(day_of_week), hours_online};
    
    // Get current model weights from metadata (or use defaults)
    std::vector<double> current_weights = {0.0, 0.0, 0.0, 0.0}; // Default weights
    if (event.computation_metadata.contains("model_weights")) {
        current_weights = event.computation_metadata["model_weights"].get<std::vector<double>>();
    }
    
    // Estimate actual logout probability (in real app, this would be observed data)
    double actual_probability = estimateLogoutProbability(current_hour, day_of_week, hours_online);
    
    // Compute gradient for logistic regression
    auto gradient = computeLogisticGradient(features, actual_probability, current_weights);
    
    DEBUG_INFO("Generated gradient for features: [" 
              << features[1] << "h, day=" << features[2] 
              << ", online=" << features[3] << "h] -> p=" << actual_probability);
    
    // Print local gradient for validation
    std::cout << "LOCAL_GRADIENT: [" << gradient[0] << ", " << gradient[1] 
              << ", " << gradient[2] << ", " << gradient[3] << "] (Client: " 
              << client_id_ << ")" << std::endl;
    
    return serializeGradient(gradient);
}

std::vector<std::string> FederatedLogoutGradientModule::shardData(const std::string& data, int num_shards) {
    DEBUG_ERROR("FederatedLogoutGradientModule requires event context for secure aggregation!");
    DEBUG_ERROR("Use shardData(data, num_shards, event) instead.");
    throw std::runtime_error("Federated learning requires event context - use extended shardData method");
}

std::vector<std::string> FederatedLogoutGradientModule::shardData(const std::string& data, int num_shards, const Event& event) {
    DEBUG_INFO("Applying secure aggregation with event context");
    
    auto gradient = deserializeGradient(data);
    if (gradient.empty()) {
        DEBUG_ERROR("Failed to deserialize gradient data");
        return {data};
    }
    
    DEBUG_INFO("Gradient before masking: [" << gradient[0] << ", " << gradient[1] << ", ...]");
    
    // Apply secure aggregation masks using participant info from event
    applySecureAggregationMasks(gradient, event);
    
    DEBUG_INFO("Gradient after masking applied");
    
    // Now shard the masked gradient
    if (num_shards <= 1) {
        return {serializeGradient(gradient)};
    }
    
    // Use additive secret sharing for secure multi-party federated learning
    // Each shard will be the same size (full gradient) and sum to the original
    
    std::vector<std::string> shards;
    std::vector<std::vector<double>> gradient_shards(num_shards);
    
    // Initialize all shards with same size as original gradient (4 parameters)
    for (int i = 0; i < num_shards; ++i) {
        gradient_shards[i].resize(gradient.size(), 0.0);
    }
    
    // Generate random shards using additive secret sharing
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(-10.0, 10.0);  // Reasonable range for gradients
    
    // For each parameter in the gradient
    for (size_t param_idx = 0; param_idx < gradient.size(); ++param_idx) {
        double sum_of_random_shards = 0.0;
        
        // Generate random values for first (num_shards-1) shards
        for (int shard_idx = 0; shard_idx < num_shards - 1; ++shard_idx) {
            double random_value = dis(gen);
            gradient_shards[shard_idx][param_idx] = random_value;
            sum_of_random_shards += random_value;
        }
        
        // Last shard ensures sum equals original gradient value
        gradient_shards[num_shards - 1][param_idx] = gradient[param_idx] - sum_of_random_shards;
    }
    
    // Serialize all shards (each is a full 4-parameter gradient)
    for (int i = 0; i < num_shards; ++i) {
        shards.push_back(serializeGradient(gradient_shards[i]));
        DEBUG_DEBUG("Shard " << i << " gradient: [" 
                  << gradient_shards[i][0] << ", " << gradient_shards[i][1] << ", ...]");
    }
    
    DEBUG_INFO("Split masked gradient into " << shards.size() << " shards");
    return shards;
}

double FederatedLogoutGradientModule::getHoursOnlineToday() const {
    // Mock implementation - in real system, would track actual login time
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto local_tm = *std::localtime(&time_t);
    
    // Simulate: been online since 9 AM, or for current_hour - 9 hours
    int hours_since_9am = std::max(0, local_tm.tm_hour - 9);
    return static_cast<double>(hours_since_9am);
}

int FederatedLogoutGradientModule::getCurrentHour() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto local_tm = *std::localtime(&time_t);
    return local_tm.tm_hour;
}

int FederatedLogoutGradientModule::getCurrentDayOfWeek() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto local_tm = *std::localtime(&time_t);
    return local_tm.tm_wday; // 0=Sunday, 1=Monday, etc.
}

double FederatedLogoutGradientModule::estimateLogoutProbability(int hour, int day_of_week, double hours_online) const {
    // Mock probability based on realistic patterns
    double prob = 0.1; // Base probability
    
    // Higher probability in evening
    if (hour >= 17) prob += 0.4;
    if (hour >= 20) prob += 0.3;
    
    // Higher probability on Friday
    if (day_of_week == 5) prob += 0.2;
    
    // Higher probability after long sessions
    if (hours_online > 8) prob += 0.3;
    if (hours_online > 10) prob += 0.2;
    
    // Add some client-specific variation
    std::hash<std::string> hasher;
    double client_variation = (hasher(client_id_) % 100) / 1000.0; // 0.0 to 0.099
    prob += client_variation;
    
    return std::min(0.9, std::max(0.05, prob)); // Clamp between 5% and 90%
}

std::vector<double> FederatedLogoutGradientModule::computeLogisticGradient(
    const std::vector<double>& features, 
    double actual_probability,
    const std::vector<double>& current_weights) const {
    
    // Logistic regression: p = sigmoid(w^T * x)
    double linear_combination = 0.0;
    for (size_t i = 0; i < features.size() && i < current_weights.size(); ++i) {
        linear_combination += current_weights[i] * features[i];
    }
    
    // Sigmoid function
    double predicted_prob = 1.0 / (1.0 + std::exp(-linear_combination));
    
    // Gradient = (predicted - actual) * features
    double error = predicted_prob - actual_probability;
    
    std::vector<double> gradient(features.size());
    for (size_t i = 0; i < features.size(); ++i) {
        gradient[i] = error * features[i];
    }
    
    return gradient;
}

std::string FederatedLogoutGradientModule::serializeGradient(const std::vector<double>& gradient) const {
    nlohmann::json j = gradient;
    return j.dump();
}

std::vector<double> FederatedLogoutGradientModule::deserializeGradient(const std::string& data) const {
    try {
        nlohmann::json j = nlohmann::json::parse(data);
        return j.get<std::vector<double>>();
    } catch (const std::exception& e) {
        DEBUG_ERROR("Failed to deserialize gradient: " << e.what());
        return {};
    }
}

std::string FederatedLogoutGradientModule::computeSharedSecret(const std::string& other_public_key, const std::string& other_client_id) const {
    // Simple deterministic shared secret from combining keys
    // In production: use proper ECDH with Ed25519 -> X25519 conversion
    std::hash<std::string> hasher;
    
    // Simple deterministic shared secret generation
    // Both clients must generate the same shared secret for their pair
    // Solution: concatenate both keys in a deterministic order
    
    // Compare keys as hex numbers to determine ordering
    // Convert key strings to numeric values for proper comparison
    size_t my_key_hash = hasher(private_key_);
    size_t their_key_hash = hasher(other_public_key);
    
    std::string combined;
    if (my_key_hash < their_key_hash) {
        // Our key has smaller hash value
        combined = private_key_ + ":" + other_public_key;
    } else {
        // Their key has smaller hash value
        combined = other_public_key + ":" + private_key_;
    }
        
    return std::to_string(hasher(combined));
}

std::vector<double> FederatedLogoutGradientModule::generateMask(const std::string& shared_secret, size_t size) const {
    std::hash<std::string> hasher;
    std::mt19937 gen(hasher(shared_secret));
    std::normal_distribution<double> dis(0.0, 0.1); // Small masks
    
    std::vector<double> mask(size);
    for (size_t i = 0; i < size; ++i) {
        mask[i] = dis(gen);
    }
    
    return mask;
}

void FederatedLogoutGradientModule::applySecureAggregationMasks(std::vector<double>& gradient, const Event& event) {
    DEBUG_INFO("Applying pairwise masks for secure aggregation");
    
    for (const auto& participant : event.participants) {
        if (participant.client_id == client_id_) continue; // Skip self
        
        auto shared_secret = computeSharedSecret(participant.ed25519_pub, participant.client_id);
        auto mask = generateMask(shared_secret, gradient.size());
        
        // Add mask if our ID is smaller, subtract if larger (ensures cancellation)
        if (client_id_ < participant.client_id) {
            for (size_t i = 0; i < gradient.size(); ++i) {
                gradient[i] += mask[i];
            }
        } else {
            for (size_t i = 0; i < gradient.size(); ++i) {
                gradient[i] -= mask[i];
            }
        }
    }
    
    DEBUG_INFO("Applied masks for " << event.participants.size() - 1 << " peers");
}