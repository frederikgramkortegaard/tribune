# TODO


## MPC Computation Execution
- [ ] Handle timeouts/missing participants (have event-level timeouts, good enough for now)

## Real-World Implementation
- [x] Demonstrated the framework by training a federated machine learning model that predicts user logout probability based on device usage patterns, enabling collaborative predictive analytics without exposing individual user behavior or raw client data
- [ ] Additional real-world use cases beyond federated learning

## Advanced Cryptographic Protocols
- [ ] Investigate homomorphic encryption for multiplication operations
- [ ] Implement Paillier encryption for additive homomorphic operations
- [ ] Support for computing on (x,y) pairs without breaking relationships
- [ ] Explore CKKS scheme for approximate arithmetic on encrypted data


## IMPLEMENTED

### Core Infrastructure
- [x] TribuneServer with event announcement system
- [x] TribuneClient with P2P coordination
- [x] JSON serialization for all protocol messages
- [x] Configurable participant selection (min/max participants)
- [x] Event-based participant lists for data integrity

### Networking & Communication  
- [x] HTTP endpoints: `/connect`, `/submit`, `/peers`, `/event`, `/peer-data`
- [x] Server announces events only to selected participants
- [x] Clients share data with other participants via P2P
- [x] Automatic UUID generation for client IDs

### Data Collection & Secret Sharing
- [x] Pluggable DataCollectionModule interface
- [x] MockDataCollectionModule for development
- [x] Client data collection on event announcement
- [x] Secret sharing implementation with shardData() method
- [x] Secure random sharding that reconstructs to original value
- [x] Plain string data format (simplified from JSON)

### MPC Computation Framework
- [x] MPCComputation abstract interface for pluggable computation types
- [x] SumComputation implementation for integer aggregation
- [x] Computation registry system in TribuneClient
- [x] Event computation_type field with JSON serialization
- [x] Flexible computation_metadata field for parameterized requests
- [x] Thread-safe shard validation and storage
- [x] Check if all expected shards received for an event (hasAllShards())
- [x] Execute registered computation when complete
- [x] Send EventResponse back to server with aggregated result

### Cryptography & Signatures
- [x] Ed25519 signature implementation
- [x] PGP-style cryptographic signatures for peer data validation
- [x] Real key generation (Ed25519 keypairs)

### Federated Learning Implementation
- [x] Implemented secure aggregation protocol using pairwise masking, where client gradients remain private through symmetric mask generation that cancels out during server-side aggregation
- [x] Built logistic regression gradient computation for collaborative model training on distributed data
- [x] Developed centralized model weight management system with periodic distribution to participating clients
- [x] Created multi-round training coordinator that orchestrates synchronous federated learning across distributed participants
- [x] Delivered complete demonstration with functional server and client applications showing real privacy-preserving ML training
- [x] Automated the entire demo workflow with Python orchestration script featuring colored real-time output monitoring
- [x] Extended DataCollectionModule interface to support event-aware sharding for secure aggregation requirements
