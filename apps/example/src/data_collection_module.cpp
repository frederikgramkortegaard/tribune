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

  // Use hash to generate a value between 10-50 for easier math verification
  int predictable_value = 10 + (client_hash % 41); // 10-50 range

  DEBUG_DEBUG("Client " << client_id_ << " generated predictable value: "
                        << predictable_value);

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
