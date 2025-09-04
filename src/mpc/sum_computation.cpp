#include "mpc/mpc_computation.hpp"
#include "utils/logging.hpp"
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>

std::string SumComputation::compute(const std::vector<std::string> &shards, 
                                   const nlohmann::json &metadata) {
  int total = 0;

  DEBUG_DEBUG("=== COMPUTING SUM ===");
  DEBUG_DEBUG("Processing " << shards.size() << " shards");

  for (const auto &shard : shards) {
    try {
      int value = std::stoi(shard);
      total += value;
      DEBUG_DEBUG("Shard value: " << value << ", Running total: " << total);
    } catch (const std::exception &e) {
      DEBUG_WARN("Could not parse shard '" << shard << "' as integer, skipping");
    }
  }

  DEBUG_DEBUG("Final sum: " << total);
  DEBUG_DEBUG("===================");

  return std::to_string(total);
}

std::string SumComputation::aggregateResults(
    const std::vector<std::string> &client_results) {
  DEBUG_DEBUG("=== AGGREGATING SUM RESULTS ===");
  DEBUG_DEBUG("Processing " << client_results.size() << " client results");

  if (client_results.empty()) {
    DEBUG_DEBUG("No client results to aggregate");
    return "0";
  }

  int total = 0;

  for (const auto &result : client_results) {
    try {
      int value = std::stoi(result);
      total += value;
      DEBUG_DEBUG("Client result: " << value << ", Running total: " << total);
    } catch (const std::exception &e) {
      DEBUG_WARN("Could not parse client result '" << result << "', skipping");
    }
  }

  DEBUG_DEBUG("Final aggregated result: " << total);
  DEBUG_DEBUG("===============================");

  return std::to_string(total);
}
