
# tribune

A secure multi-party computation system with star topology networking.

**Note:** This uses a star topology with centralized coordination, which has obvious trust and single-point-of-failure issues. We assume an honest-but-curious adversary model - the server coordinates but doesn't see raw client data. Not suitable for truly adversarial environments where you can't trust the coordinator.

## Architecture

The `src/` directory contains the core library for distributed MPC. Public API is just two classes:
- **TribuneServer** - Coordinates computation events and manages client roster
- **TribuneClient** - Connects to servers and handles computation events

Everything else (protocol parsing, JSON serialization, client state management) is internal implementation.

The `apps/` directory has specific applications that use the library:
- `server_app` - Basic server that announces computation events every 40 seconds
- `client_app` - Simple client that connects and listens for events
- `mpc-ai-training` - (planned) Train ML models like linear regression using MPC so raw data never leaves client machines

## Product Vision

Regular p2p system, but event-driven. Server publishes computation specs (could be SQL table structures or ML model definitions), clients subscribe with their endpoints, then we get this publisher-consumer MPC system where data stays distributed.

Future: integrate blockchain payments for computation contributions.