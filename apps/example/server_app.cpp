#include "server/tribune_server.hpp"
#include "events/events.hpp"
#include "mpc/mpc_computation.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <memory>

int main() {
    std::cout << "Starting Tribune Server..." << std::endl;
    
    TribuneServer server("localhost", 8080);
    
    // Register the sum computation
    server.registerComputation("sum", std::make_unique<SumComputation>());
    
    // Run server in background thread
    std::thread server_thread([&server]() {
        server.start();  // This blocks in its thread
    });
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Server started! Ready to accept connections." << std::endl;
    
    // Main thread sends events periodically
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(40));
        
        std::string event_id = "event-" + std::to_string(dis(gen));
        
        if (auto event = server.createEvent(EventType::DataSubmission, event_id)) {
            std::cout << "Created event: " << event_id << " with " 
                      << event->participants.size() << " participants" << std::endl;
            server.announceEvent(*event);
        } else {
            std::cout << "Insufficient participants for event " << event_id 
                      << ", skipping..." << std::endl;
        }
    }
    
    server_thread.join();  // Never reached but good practice
    return 0;
}