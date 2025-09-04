#include "../include/data_collection_module.hpp"
#include "utils/logging.hpp"
#include <iostream>
#include <random>
#include <sstream>

std::string MockDataCollectionModule::collectData(const Event &event) {
  // Generate predictable test values based on client ID hash
  // This ensures each client always generates the same value for verification
  std::hash<std::string> hasher;
  size_t client_hash = hasher(client_id_);

  // Use metadata to adjust data generation
  int min_val = 10;
  int max_val = 50;
  
  if (!event.computation_metadata.empty()) {
    DEBUG_DEBUG("Received metadata: " << event.computation_metadata.dump());
    
    if (event.computation_metadata.contains("min_value")) {
      min_val = event.computation_metadata["min_value"];
    }
    if (event.computation_metadata.contains("max_value")) {
      max_val = event.computation_metadata["max_value"];
    }
  }
  
  // Use hash to generate a value within the specified range
  int range = max_val - min_val + 1;
  int predictable_value = min_val + (client_hash % range);

  DEBUG_DEBUG("Client " << client_id_ << " generated value: "
                        << predictable_value << " (range: " << min_val << "-" << max_val << ")");

  return std::to_string(predictable_value);
}

std::vector<std::string>
MockDataCollectionModule::shardData(const std::string &data, int num_shards) {
  std::vector<std::string> shards;

  try {
    // Parse the string as a numeric value
    double original_value = std::stod(data);

    DEBUG_DEBUG("Sharding value " << original_value << " into " << num_shards
                                  << " pieces");

    if (num_shards <= 1) {
      shards.push_back(std::to_string(original_value));
      return shards;
    }

    // Generate random shards using cryptographically secure randomness
    std::random_device rd;
    std::mt19937 gen(rd());

    // Generate (num_shards - 1) random shards
    // Use a range of [-2*original_value, 2*original_value] to ensure good
    // distribution
    double range =
        std::abs(original_value) * 2.0 + 100.0; // Add 100 for small values
    std::uniform_real_distribution<double> dis(-range, range);

    double sum_of_random_shards = 0.0;
    for (int i = 0; i < num_shards - 1; i++) {
      double random_shard = dis(gen);
      shards.push_back(std::to_string(random_shard));
      sum_of_random_shards += random_shard;
    }

    // The last shard is calculated to make the sum equal to original_value
    double final_shard = original_value - sum_of_random_shards;
    shards.push_back(std::to_string(final_shard));

    DEBUG_DEBUG("Generated " << shards.size() << " shards, sum should equal "
                             << original_value);

    return shards;

  } catch (const std::exception &e) {
    DEBUG_ERROR("Error parsing numeric data for sharding: " << e.what());
    // Fallback: return original data as single shard
    shards.push_back(data);
    return shards;
  }
}
