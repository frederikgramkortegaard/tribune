#include "server/tribune_server.hpp"
#include "mpc/secure_sum.hpp"
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    // Load configuration
    ServerConfig config("server.json");
    
    // Create server instance
    TribuneServer server("localhost", 8080, config);
    
    // Register MPC modules
    auto secure_sum = std::make_unique<SecureSumModule>();
    server.registerModule("secure_sum", std::move(secure_sum));
    
    std::cout << "Starting Tribune server with Secure Sum module..." << std::endl;
    std::cout << "Listening on localhost:8080" << std::endl;
    
    // Start the server (blocks)
    server.start();
    
    return 0;
}