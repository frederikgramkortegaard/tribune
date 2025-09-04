
# Tribune - Distributed Multi-Party Computation Library

Tribune is a C++ library for distributed multi-party computation with P2P networking, enabling privacy-preserving machine learning training without sharing raw data. Built around a star topology architecture where clients coordinate through a central server while keeping their data local.

The system allows multiple parties to collaboratively train ML models, perform secure aggregations, and run distributed computations where no single party ever sees the complete dataset. Applications include federated learning scenarios, collaborative analytics, and any use case requiring computation over distributed private data.

:speech_balloon: **Security Model:** This uses a star topology with centralized coordination, which has obvious trust and single-point-of-failure issues. We assume an honest-but-curious adversary model - the server coordinates but doesn't see raw client data. Not suitable for truly adversarial environments where you can't trust the coordinator.

:pushpin: [Development TODO](https://github.com/frederikgramkortegaard/tribune/blob/master/TODO.md)

:page_facing_up: [Design Decisions](https://github.com/frederikgramkortegaard/tribune/blob/master/DESIGN_DECISIONS.md)

## Architecture

The `src/` directory contains the core library for distributed MPC. Public API includes:
- **TribuneServer** - Coordinates computation events and manages client roster
- **TribuneClient** - Connects to servers and handles computation events
- **MPCComputation** - Pluggable interface for different computation types (sum, average, ML training, etc.)

Everything else (protocol parsing, JSON serialization, client state management) is internal implementation.

The `apps/` directory has specific applications that use the library:
- `server_app` - Basic server that announces computation events every 40 seconds
- `client_app` - Simple client that connects and listens for events
- `mpc-ai-training` - (planned) Train ML models like linear regression using MPC so raw data never leaves client machines

## Product Vision

Regular p2p system, but event-driven. Server publishes computation specs (could be SQL table structures or ML model definitions), clients subscribe with their endpoints, then we get this publisher-consumer MPC system where data stays distributed.

Future: integrate blockchain payments for computation contributions.

## Quick Start

### Build and Run Demo

```bash
# Using CMake (recommended)
mkdir build && cd build
cmake ..
make

# Or using Makefile (simple)
make all

# Run the full MPC demonstration
make demo
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

**Debug Build Features:**
- `DEBUG_INFO`: General information about operations
- `DEBUG_DEBUG`: Detailed tracing of computation steps, event processing
- `DEBUG_WARN`: Warnings about parsing errors or invalid data  
- `DEBUG_ERROR`: Error conditions that don't cause termination
- `LOG`: Always-on messages (works in both debug and release builds)

**Release Build** (default): All debug macros become no-ops for optimal performance.

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
    // Must match what clients register
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
#include "data/simple_data_collection.hpp"

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
    // or anything similar, it will be polled whenver the client receives an Announcement
    client.setDataCollectionModule(std::make_unique<SimpleDataCollectionModule>());
    
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

### Manual Testing

```bash
# Terminal 1: Start server
./server_app

# Terminal 2-N: Start clients on different ports  
./client_app 9001 private_key_1 public_key_1
./client_app 9002 private_key_2 public_key_2
# ... etc
```

### Demo Script Features

The `scripts/run_demo.py` script automatically:
- Generates unique Ed25519 keypairs for each client
- Spawns 1 server + 10 clients with colored output
- Shows real-time MPC computation results
- Handles graceful shutdown with Ctrl+C
