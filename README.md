# Tribune

A distributed Multi-Party Computation (MPC) system for secure collaborative data processing.

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