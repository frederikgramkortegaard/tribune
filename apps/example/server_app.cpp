#include "events/events.hpp"
#include "mpc/mpc_computation.hpp"
#include "server/tribune_server.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <random>
#include <thread>

int main() {
  std::cout << "Starting Tribune Server..." << std::endl;
  std::cout << "DEBUG: Server executable updated with signature support"
            << std::endl;

  TribuneServer server("localhost", 8080);

  // Register the sum computation
  server.registerComputation("sum", std::make_unique<SumComputation>());

  // Run server in background thread
  std::thread server_thread([&server]() {
    server.start(); // This blocks in its thread
  });

  // Give server time to start
  std::this_thread::sleep_for(std::chrono::seconds(1));
  std::cout << "Server started! Ready to accept connections." << std::endl;

  // Wait for clients to connect, then send a single event for testing
  std::this_thread::sleep_for(std::chrono::seconds(15));

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(1000, 9999);
  std::string event_id = "event-" + std::to_string(dis(gen));

  std::cout << "DEBUG: About to create and announce single test event..."
            << std::endl;

  std::string result;
  if (auto event = server.createEvent(EventType::DataRequestEvent, event_id)) {
    // Add metadata to demonstrate flexible data requests
    event->computation_metadata = {
        {"date_range", "2024-01-01 to 2024-12-31"},
        {"min_value", 10},
        {"max_value", 100}
    };
    
    std::cout << "Created event: " << event_id << " with "
              << event->participants.size() << " participants" << std::endl;
    std::cout << "Metadata: " << event->computation_metadata.dump() << std::endl;
    server.announceEvent(*event, &result);
    std::cout << "Event announced. Waiting for completion..." << std::endl;
    
    // Wait for computation to complete
    while (result.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    std::cout << "=== COMPUTATION COMPLETED ===" << std::endl;
    std::cout << "Final Result: " << result << std::endl;
  } else {
    std::cout << "Insufficient participants for event " << event_id
              << ", skipping..." << std::endl;
  }

  // Keep server running to process responses
  std::this_thread::sleep_for(std::chrono::seconds(180));

  server_thread.join(); // Never reached but good practice
  return 0;
}
