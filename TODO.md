# TODO


## MPC Computation Execution
- [ ] Check if all expected shards received for an event
- [ ] Handle timeouts/missing participants

## Crypto + MPC
- [ ] Pick crypto lib (libsodium?)
- [ ] Try to implement a real usecase using a non-mock data collection module

## Data Formats
- [ ] JSON schemas for different computation types

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
- [x] Thread-safe shard validation and storage
- [x] Execute registered computation when complete
- [x] Send EventResponse back to server with aggregated result

### Cryptography & Signatures
- [x] Ed25519 signature implementation
- [x] PGP-style cryptographic signatures for peer data validation
- [x] Real key generation (Ed25519 keypairs)
