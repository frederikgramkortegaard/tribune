#pragma once
#include "client/data_collection_module.hpp"
#include <chrono>
#include <vector>
#include <string>

class FederatedLogoutGradientModule : public DataCollectionModule {
public:
    FederatedLogoutGradientModule(const std::string& client_id, 
                                 const std::string& private_key);
    
    std::string collectData(const Event& event) override;
    std::vector<std::string> shardData(const std::string& data, int num_shards) override;
    
    // Extended version with event context for secure aggregation
    std::vector<std::string> shardData(const std::string& data, int num_shards, const Event& event);
    
private:
    std::string client_id_;
    std::string private_key_;
    
    // Screen time tracking
    double getHoursOnlineToday() const;
    int getCurrentHour() const;
    int getCurrentDayOfWeek() const; // 0=Sunday, 1=Monday, etc.
    
    // Logout probability estimation (mock for demo)
    double estimateLogoutProbability(int hour, int day_of_week, double hours_online) const;
    
    // Gradient computation for logistic regression
    std::vector<double> computeLogisticGradient(const std::vector<double>& features, 
                                               double actual_probability,
                                               const std::vector<double>& current_weights) const;
    
    // Secure aggregation with pairwise masking
    void applySecureAggregationMasks(std::vector<double>& gradient, 
                                    const Event& event);
    
    // Serialization helpers
    std::string serializeGradient(const std::vector<double>& gradient) const;
    std::vector<double> deserializeGradient(const std::string& data) const;
    
    // Utility for shared secret generation
    std::string computeSharedSecret(const std::string& other_public_key) const;
    std::vector<double> generateMask(const std::string& shared_secret, size_t size) const;
};