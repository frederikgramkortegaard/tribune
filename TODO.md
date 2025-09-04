# TODO

IMPORTANT : When sharding data to peers we need to randomly split it, such that not every peer receives an even share
no_data_module_configured

periodic cleanup counter should be defined in a config  ( in on peerdatareceived, we even have another cleanup method so why is there cleanup here?)
possibly read locks in hasAllShards() ?

## MPC Computation Execution
- [ ] Check if all expected shards received for an event
- [ ] Execute registered computation when complete
- [ ] Send EventResponse back to server with aggregated result
- [ ] Handle timeouts/missing participants
- [ ] Process orphan shards when event announcement arrives

## Crypto + MPC
- [ ] Pick crypto lib (libsodium?)
- [ ] Secret sharing implementation  
- [ ] Actual MPC computation logic instead of dummy data
- [ ] Real key generation (not dummy keys)
- [ ] PGP-style cryptographic signatures for peer data validation

## Data Formats
- [ ] Better structured data instead of raw strings
- [ ] JSON schemas for different computation types
- [ ] Share encoding/decoding

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

### Data Collection
- [x] Pluggable DataCollectionModule interface
- [x] MockDataCollectionModule for development
- [x] Client data collection on event announcement

### MPC Computation Framework
- [x] MPCComputation abstract interface for pluggable computation types
- [x] SumComputation implementation for integer aggregation
- [x] Computation registry system in TribuneClient
- [x] Event computation_type field with JSON serialization
- [x] Thread-safe shard validation and storage with orphan handling
