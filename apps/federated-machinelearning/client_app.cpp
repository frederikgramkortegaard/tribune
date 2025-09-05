#include "events/events.hpp"
#include "client/tribune_client.hpp"
#include "include/federated_logout_module.hpp"
#include "include/federated_computation.hpp"
#include "crypto/signature.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <random>

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string server_host = "localhost";
    int server_port = 8080;
    int listen_port = 9001;
    
    if (argc >= 2) listen_port = std::stoi(argv[1]);
    if (argc >= 3) server_host = argv[2];
    if (argc >= 4) server_port = std::stoi(argv[3]);
    
    std::cout << "Starting Federated Learning Client..." << std::endl;
    std::cout << "Listen Port: " << listen_port << std::endl;
    std::cout << "Server: " << server_host << ":" << server_port << std::endl;
    
    // Generate Ed25519 keypair
    std::cout << "Generating Ed25519 keypair..." << std::endl;
    auto keypair = SignatureUtils::generateKeyPair();
    std::string public_key = keypair.first;
    std::string private_key = keypair.second;
    std::cout << "Generated public key: " << public_key << std::endl;
    
    // Create client with keys
    TribuneClient client(server_host, server_port, listen_port, private_key, public_key);
    
    // Register the federated computation
    client.registerComputation("federated_aggregation", std::make_unique<FederatedAggregationComputation>());
    
    // Set up federated data collection module
    auto logout_module = std::make_unique<FederatedLogoutGradientModule>(client.getClientId(), private_key);
    client.setDataCollectionModule(std::move(logout_module));
    
    std::cout << "Registered federated logout gradient module" << std::endl;
    std::cout << "Client ready for federated learning!" << std::endl;
    
    // Connect to the seed node (server)
    if (!client.connectToSeed()) {
        std::cout << "Failed to connect to server. Exiting." << std::endl;
        return 1;
    }
    
    std::cout << "Connected to server successfully!" << std::endl;
    
    // Start listening for events from peers
    client.startListening();
    std::cout << "Started listening for events. Waiting for training rounds..." << std::endl;
    
    // Keep client running to participate in training rounds
    int heartbeat_counter = 0;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        heartbeat_counter++;
        
        // Print a heartbeat every 30 seconds to show the client is alive
        if (heartbeat_counter % 6 == 0) {
            std::cout << "Client heartbeat - waiting for training events..." << std::endl;
        }
    }
    
    return 0;
}