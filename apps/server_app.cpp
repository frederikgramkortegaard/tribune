#include "server/tribune_server.hpp"
#include "events/events.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

int main() {
    std::cout << "Starting Tribune Server..." << std::endl;
    
    TribuneServer server("localhost", 8080);
    
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
        
        Event e;
        e.type_ = EventType::DataSubmission;
        e.event_id = "event-" + std::to_string(dis(gen));
        
        std::cout << "Announcing event: " << e.event_id << std::endl;
        server.announceEvent(e);
    }
    
    server_thread.join();  // Never reached but good practice
    return 0;
}