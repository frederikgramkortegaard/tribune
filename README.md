# Tribune

A distributed Multi-Party Computation (MPC) system for secure collaborative data processing.
# Tribune - Distributed Multi-Party Computation Library

Tribune is a C++ library for distributed multi-party computation with P2P networking, enabling privacy-preserving machine learning training without sharing raw data. Built around a star topology architecture where clients coordinate through a central server while keeping their data local.

The system allows multiple parties to collaboratively train ML models, perform secure aggregations, and run distributed computations where no single party ever sees the complete dataset. Applications include federated learning scenarios, collaborative analytics, and any use case requiring computation over distributed private data.

:speech_balloon: **Security Model:** This uses a star topology with centralized coordination, which has obvious trust and single-point-of-failure issues. We assume an honest-but-curious adversary model - the server coordinates but doesn't see raw client data. Not suitable for truly adversarial environments where you can't trust the coordinator.

## Federated Learning Demo

This repository includes a **complete federated learning implementation** demonstrating privacy-preserving collaborative model training. Multiple clients train a shared logistic regression model using secure aggregation with pairwise masking - ensuring no client ever sees another's data.

**Quick demo:** `python3 apps/federated-machinelearning/scripts/run_demo.py`  
*(The script will automatically build the executables if needed - requires CMake and a C++ compiler)*

Work In Progrss Notice: The implementation shows real-world federated learning with gradient computation, secure aggregation, and distributed training rounds. See [`apps/federated-machinelearning/`](apps/federated-machinelearning/) for details.

## Architecture

The `src/` directory contains the core library for distributed MPC. Public API includes:
- **TribuneServer** - Coordinates computation events, manages client roster, and aggregates results
- **TribuneClient** - Connects to servers, handles computation events, and participates in P2P data sharing
- **MPCComputation** - Pluggable interface for different computation types (sum, average, ML training, etc.)
- **DataCollectionModule** - Interface for clients to provide data (must be implemented by applications)

Everything else (protocol parsing, JSON serialization, client state management, crypto utilities, logging) is internal implementation.

The `apps/` directory contains example applications:

**`apps/example/`** - Basic MPC demonstration:
- `server_app` - Basic server that creates and announces computation events
- `client_app` - Simple client that connects and listens for events
- `MockDataCollectionModule` - Example data collection implementation for testing

**`apps/federated-machinelearning/`** - Privacy-preserving federated learning demo:
- Demonstrates collaborative training of a logistic regression model
- Implements secure aggregation with pairwise masking
- Includes complete working example with multiple clients training a logout prediction model
- Run with: `python3 apps/federated-machinelearning/scripts/run_demo.py`


## Quick Start

### Prerequisites
- C++20 compatible compiler
- CMake 3.15+
- OpenSSL (for TLS support)
- libsodium (for Ed25519 signatures)

### Setup

1. **Generate TLS Certificates (Required)**
   
   Tribune uses TLS by default for secure communication. Generate certificates before running:
   ```bash
   ./generate_certs.sh
   ```
   
   This creates self-signed certificates in the `certs/` directory.

2. **Build**
   ```bash
   cmake .
   make
   ```

3. **Configure**
   
   Configuration files use TLS by default:
   - `server.json` - Server configuration
   - `client.json` - Client configuration

4. **Run**
   ```bash
   # Start server
   ./tribune_server
   
   # Start clients (in separate terminals)
   ./tribune_client
   ```

## Security

Tribune implements:
- **TLS/HTTPS** for all network communication
- **Ed25519** signatures for message authentication
- **MPC protocols** for privacy-preserving computation

## Configuration

### Server Configuration (`server.json`)
```json
{
  "host": "localhost",
  "port": 8080,
  "use_tls": true,
  "cert_file": "certs/server-cert.pem",
  "private_key_file": "certs/server-key.pem"
}
```

### Client Configuration (`client.json`)
```json
{
  "server_host": "localhost",
  "server_port": 8080,
  "use_tls": true,
  "verify_server_cert": false
}
```

Set `verify_server_cert: true` for production with valid certificates.

## Development

For development without TLS, set `"use_tls": false` in both config files.

## Architecture

Tribune uses a server-orchestrated, peer-to-peer MPC architecture:
- **Server**: Coordinates events and participant selection
- **Clients**: Perform distributed computations
- **P2P**: Direct client-to-client data sharing for MPC protocols
