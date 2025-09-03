#include "client/tribune_client.hpp"
#include "mpc/mpc_computation.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string server_host = "localhost";
    int server_port = 8080;
    int listen_port = 9001;
    std::string private_key = "dummy_private_key";
    std::string public_key = "dummy_public_key";
    
    if (argc >= 2) listen_port = std::stoi(argv[1]);
    if (argc >= 3) private_key = argv[2];
    if (argc >= 4) public_key = argv[3];
    if (argc >= 5) server_host = argv[4];
    if (argc >= 6) server_port = std::stoi(argv[5]);
    
    std::cout << "Starting Tribune Client..." << std::endl;
    std::cout << "Listen Port: " << listen_port << std::endl;
    std::cout << "Server: " << server_host << ":" << server_port << std::endl;
    std::cout << "Public Key: " << public_key << std::endl;
    
    // Create client with keys
    TribuneClient client(server_host, server_port, listen_port, private_key, public_key);
    
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