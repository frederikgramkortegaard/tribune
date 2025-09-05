#include "events/events.hpp"
#include "server/tribune_server.hpp"
#include "include/federated_computation.hpp"
#include <chrono>
#include <iostream>
#include <iomanip>
#include <memory>
#include <random>
#include <thread>

int main() {
    std::cout << "Starting Federated Learning Server..." << std::endl;
    
    TribuneServer server("localhost", 8080);
    
    // Create federated aggregation computation with learning rate
    auto federated_comp = std::make_unique<FederatedAggregationComputation>(0.01);
    
    // Initialize model weights for logout prediction
    // Features: [bias, hour, day_of_week, hours_online]
    std::vector<double> initial_weights = {0.1, -0.05, 0.02, 0.15}; // Small random initial values
    federated_comp->setModelWeights(initial_weights);
    
    std::cout << "Initialized logout prediction model with weights: [";
    for (size_t i = 0; i < initial_weights.size(); ++i) {
        std::cout << initial_weights[i];
        if (i < initial_weights.size() - 1) std::cout << ", ";
    }
    std::cout << "]" << std::endl;
    
    // Keep a reference before registering (since we'll move it)
    FederatedAggregationComputation* federated_ref = federated_comp.get();
    
    // Register the federated computation
    server.registerComputation("federated_aggregation", std::move(federated_comp));
    
    // Run server in background thread
    std::thread server_thread([&server]() {
        server.start();
    });
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "Server started! Waiting for clients to connect..." << std::endl;
    
    // Wait for clients to connect
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // Run multiple training rounds
    const int num_rounds = 5;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    for (int round = 1; round <= num_rounds; ++round) {
        std::cout << "\n=== TRAINING ROUND " << round << " ===" << std::endl;
        
        std::string event_id = "training-round-" + std::to_string(round) + "-" + std::to_string(dis(gen));
        
        std::string result;
        if (auto event = server.createEvent(EventType::DataRequestEvent, event_id, "federated_aggregation")) {
            // Get current model weights from the registered computation
            auto current_weights = federated_ref->getModelWeights();
            
            // Add metadata for federated learning
            event->computation_metadata = {
                {"round", round},
                {"model_weights", current_weights},
                {"learning_rate", 0.01},
                {"gradient_size", current_weights.size()}
            };
            
            std::cout << "Round " << round << " - Current weights: [";
            for (size_t i = 0; i < current_weights.size(); ++i) {
                std::cout << std::fixed << std::setprecision(4) << current_weights[i];
                if (i < current_weights.size() - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;
            
            server.announceEvent(*event, &result);
            std::cout << "Event announced. Waiting for federated aggregation..." << std::endl;
            
            // Wait for computation to complete
            auto start_time = std::chrono::steady_clock::now();
            while (result.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                // Timeout after 30 seconds
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                if (elapsed > std::chrono::seconds(30)) {
                    std::cout << "Round " << round << " timed out!" << std::endl;
                    break;
                }
            }
            
            if (!result.empty()) {
                std::cout << "=== ROUND " << round << " COMPLETED ===" << std::endl;
                std::cout << "Updated model returned from aggregation" << std::endl;
                
                // Get updated weights after aggregation
                auto updated_weights = federated_ref->getModelWeights();
                std::cout << "New weights: [";
                for (size_t i = 0; i < updated_weights.size(); ++i) {
                    std::cout << std::fixed << std::setprecision(4) << updated_weights[i];
                    if (i < updated_weights.size() - 1) std::cout << ", ";
                }
                std::cout << "]" << std::endl;
            }
        } else {
            std::cout << "Insufficient participants for round " << round << ", skipping..." << std::endl;
        }
        
        // Wait between rounds
        if (round < num_rounds) {
            std::cout << "Waiting before next round..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    std::cout << "\n=== TRAINING COMPLETE ===" << std::endl;
    std::cout << "Federated logout prediction model trained!" << std::endl;
    
    // Keep server running
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    server_thread.join();
    return 0;
}