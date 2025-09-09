#include "client/tribune_client.hpp"
#include "client/simple_data_module.hpp"
#include "mpc/secure_sum.hpp"
#include <iostream>
#include <memory>
#include <thread>

int main(int argc, char* argv[]) {
    // Load configuration
    ClientConfig config("client.json");
    
    // Generate Ed25519 keypair
    auto keypair = SignatureUtils::generateKeyPair();
    
    // Create client instance
    // Args: server_host, server_port, listen_host, listen_port, private_key, public_key, config
    TribuneClient client("localhost", 8080, "localhost", 0, 
                        keypair.second, keypair.first, config);
    
    // Register MPC modules
    auto secure_sum = std::make_unique<SecureSumModule>();
    client.registerModule("secure_sum", std::move(secure_sum));
    
    // Set data collection module (required before starting)
    auto data_module = std::make_unique<SimpleDataModule>();
    client.setDataCollectionModule(std::move(data_module));
    
    std::cout << "Starting Tribune client with Secure Sum module..." << std::endl;
    
    // Connect to server
    if (client.connectToSeed()) {
        std::cout << "Connected to server successfully!" << std::endl;
        
        // Start listening for events
        client.startListening();
        
        // Keep running
        std::cout << "Client running. Press Ctrl+C to stop." << std::endl;
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } else {
        std::cerr << "Failed to connect to server!" << std::endl;
        return 1;
    }
    
    return 0;
}