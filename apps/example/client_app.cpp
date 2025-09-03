#include "client/tribune_client.hpp"
#include "mpc/mpc_computation.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

int main() {
    std::cout << "Starting Tribune Client..." << std::endl;
    
    // Create client that connects to server at localhost:8080 and listens on port 9001
    TribuneClient client("localhost", 8080, 9001);
    
    // Register the sum computation
    client.registerComputation("sum", std::make_unique<SumComputation>());
    
    // Connect to the seed node (server)
    if (!client.connectToSeed()) {
        std::cout << "Failed to connect to seed node. Exiting." << std::endl;
        return 1;
    }
    
    // Start listening for events in background
    client.startListening();
    
    std::cout << "Client is running. Listening for events..." << std::endl;
    std::cout << "Press Ctrl+C to exit" << std::endl;
    
    // Keep the main thread alive
    try {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cout << "Exception in main loop: " << e.what() << std::endl;
    }
    
    return 0;
}