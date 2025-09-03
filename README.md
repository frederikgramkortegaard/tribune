
# Tribune - Distributed Multi-Party Computation Library

Tribune is a C++ library for distributed multi-party computation with P2P networking, enabling privacy-preserving machine learning training without sharing raw data. Built around a star topology architecture where clients coordinate through a central server while keeping their data local.

The system allows multiple parties to collaboratively train ML models, perform secure aggregations, and run distributed computations where no single party ever sees the complete dataset. Applications include federated learning scenarios, collaborative analytics, and any use case requiring computation over distributed private data.

**Security Model:** This uses a star topology with centralized coordination, which has obvious trust and single-point-of-failure issues. We assume an honest-but-curious adversary model - the server coordinates but doesn't see raw client data. Not suitable for truly adversarial environments where you can't trust the coordinator.

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

:pushpin: [Development TODO](https://github.com/frederikgramkortegaard/tribune/blob/master/TODO.md)