#include "mpc/mpc_computation.hpp"
#include <iostream>
#include <stdexcept>

std::string SumComputation::compute(const std::vector<std::string>& shards) {
    int total = 0;
    
    std::cout << "=== COMPUTING SUM ===" << std::endl;
    std::cout << "Processing " << shards.size() << " shards" << std::endl;
    
    for (const auto& shard : shards) {
        try {
            int value = std::stoi(shard);
            total += value;
            std::cout << "Shard value: " << value << ", Running total: " << total << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not parse shard '" << shard << "' as integer, skipping" << std::endl;
        }
    }
    
    std::cout << "Final sum: " << total << std::endl;
    std::cout << "===================" << std::endl;
    
    return std::to_string(total);
}

std::string SumComputation::aggregateResults(const std::vector<std::string>& client_results) {
    std::cout << "=== AGGREGATING SUM RESULTS ===" << std::endl;
    std::cout << "Processing " << client_results.size() << " client results" << std::endl;
    
    // For sum computation, all clients should have computed the same result
    // So we can take the first valid result or verify they all match
    if (client_results.empty()) {
        std::cout << "No client results to aggregate" << std::endl;
        return "0";
    }
    
    int first_result = 0;
    bool have_result = false;
    
    for (const auto& result : client_results) {
        try {
            int value = std::stoi(result);
            std::cout << "Client result: " << value << std::endl;
            
            if (!have_result) {
                first_result = value;
                have_result = true;
            } else if (first_result != value) {
                std::cout << "Warning: Client results differ! Expected " << first_result 
                          << " but got " << value << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Warning: Could not parse client result '" << result << "', skipping" << std::endl;
        }
    }
    
    std::cout << "Final aggregated result: " << first_result << std::endl;
    std::cout << "===============================" << std::endl;
    
    return std::to_string(first_result);
}