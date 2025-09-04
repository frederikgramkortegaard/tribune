
# Tribune - Distributed Multi-Party Computation Library

Tribune is a C++ library for distributed multi-party computation with P2P networking, enabling privacy-preserving machine learning training without sharing raw data. Built around a star topology architecture where clients coordinate through a central server while keeping their data local.

The system allows multiple parties to collaboratively train ML models, perform secure aggregations, and run distributed computations where no single party ever sees the complete dataset. Applications include federated learning scenarios, collaborative analytics, and any use case requiring computation over distributed private data.

:speech_balloon: **Security Model:** This uses a star topology with centralized coordination, which has obvious trust and single-point-of-failure issues. We assume an honest-but-curious adversary model - the server coordinates but doesn't see raw client data. Not suitable for truly adversarial environments where you can't trust the coordinator.

:pushpin: [Development TODO](https://github.com/frederikgramkortegaard/tribune/blob/master/TODO.md)

:page_facing_up: [Design Decisions](https://github.com/frederikgramkortegaard/tribune/blob/master/DESIGN_DECISIONS.md)

## Architecture

The `src/` directory contains the core library for distributed MPC. Public API includes:
- **TribuneServer** - Coordinates computation events, manages client roster, and aggregates results
- **TribuneClient** - Connects to servers, handles computation events, and participates in P2P data sharing
- **MPCComputation** - Pluggable interface for different computation types (sum, average, ML training, etc.)
- **DataCollectionModule** - Interface for clients to provide data (must be implemented by applications)

Everything else (protocol parsing, JSON serialization, client state management, crypto utilities, logging) is internal implementation.

The `apps/example/` directory has specific applications that use the library:
- `server_app` - Basic server that creates and announces computation events
- `client_app` - Simple client that connects and listens for events
- `MockDataCollectionModule` - Example data collection implementation for testing

## Product Vision

Regular p2p system, but event-driven. Server publishes computation specs (could be SQL table structures or ML model definitions), clients subscribe with their endpoints, then we get this publisher-consumer MPC system where data stays distributed.

Future: integrate blockchain payments for computation contributions.

## Quick Start

### Build

```bash
# Using CMake (recommended)
mkdir build && cd build
cmake ..
make

# Or using Makefile (simple)
make all
```

### Debug Build with Logging

Tribune includes a comprehensive logging system that can be enabled for debugging:

```bash
# Build in Debug mode to enable logging
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run with debug output
./server_app
./client_app 9001 private_key_1 public_key_1
```

**Release Build** (default): Optimized for performance with minimal logging.

```bash
# Release build (default) - minimal logging
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Simple Server Example

```cpp
#include "server/tribune_server.hpp"
#include "mpc/sum_computation.hpp"

int main() {
    // Create server on localhost:8080
    // The server coordinates MPC computations and manages client rosters
    TribuneServer server("localhost", 8080);
    
    // Register computation type - this defines what kind of MPC computation
    // the server can coordinate (sum, average, ML training, etc.)
    // only clients who also register this, can respond to "sum" requests
    server.registerComputation("sum", std::make_unique<SumComputation>());
    
    // Start server in background thread to handle client connections
    // and HTTP requests for event announcements
    std::thread server_thread([&server]() {
        server.start();
    });
    
    // Wait for clients to connect and register with the server
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // Create and announce a computation event
    // This selects participants and sends event announcements to all selected clients
    std::string result;
    if (auto event = server.createEvent(EventType::DataSubmission, "my-event")) {
        server.announceEvent(*event, &result);
        
        // Wait for computation to complete
        while (result.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        std::cout << "MPC Computation Result: " << result << std::endl;
    }
    
    server_thread.join();
    return 0;
}
```

### Simple Client Example

```cpp
#include "client/tribune_client.hpp"
#include "mpc/sum_computation.hpp"
#include "client/data_collection_module.hpp" // For DataCollectionModule interface

int main() {
    // Create client connecting to localhost:8080, listening on port 18001
    TribuneClient client("localhost", 8080, 18001, 
                        "ed25519_private_key", "ed25519_public_key");
    
    // Register computation type, these are MPC compliant
    // computation modules, of which it's also possible to create your own
    // this one just computes the sum of the shards across an event
    client.registerComputation("sum", std::make_unique<SumComputation>());

    
    // Register data collection module (how client fetches its data)
    // this could be a custom SQL query interface, an HTTP connection to temperature sensor
    // or anything similar, it will be polled whenever the client receives an Announcement
    // You must implement your own DataCollectionModule subclass
    client.setDataCollectionModule(std::make_unique<YourDataCollectionModule>());
    
    // Connect to server
    if (!client.connectToSeed()) {
        std::cout << "Failed to connect to server" << std::endl;
        return 1;
    }
    
    // Start listening for events
    client.startListening();
    
    // Keep running to participate in computations
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    return 0;
}
```

### Testing the Network

To test out the network, run the demo script:

```bash
python3 scripts/run_demo.py
```

The script automatically generates unique Ed25519 keypairs for each client, spawns 1 server + 10 clients with colored output, shows real-time MPC computation results, and handles graceful shutdown with Ctrl+C.

### DataCollectionModule Implementation Example

Here's a practical example showing how to implement all three methods:

```cpp
#include "client/data_collection_module.hpp"
#include <random>
#include <numeric>

class TemperatureSensorModule : public DataCollectionModule {
public:
    // Collect temperature data from sensor
    std::string collectData(const Event& event) override {
        double temperature = readTemperatureSensor(); // e.g., 23.5°C
        return std::to_string(temperature);
    }
    
    // Split temperature into cryptographically secure shards
    std::vector<std::string> shardData(const std::string& data, int num_shards) override {
        double value = std::stod(data);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dis(-100.0, 100.0);
        
        std::vector<std::string> shards;
        double sum = 0.0;
        
        // Generate random shards
        for (int i = 0; i < num_shards - 1; i++) {
            double shard = dis(gen);
            shards.push_back(std::to_string(shard));
            sum += shard;
        }
        
        // Final shard ensures sum equals original value
        shards.push_back(std::to_string(value - sum));
        return shards;
    }
    
    // Compute average temperature across all multi-party computation sub-results
    std::string aggregateData(const Event& event, 
                            const std::vector<std::string>& peer_data) override {

        // return sum of peer_data ....
    }
    
private:
    double readTemperatureSensor() {
        // Mock sensor reading
        return 20.0 + (rand() % 20); // 20-40°C
    }
};
```
