# TODO

## Client Response Tracking
- [ ] Add `std::unordered_map<std::string, std::unordered_map<std::string, PeerDataMessage>> received_peer_data` to client  
- [ ] Periodic check if all participants responded for each event
- [ ] Send EventResponse back to server when aggregation complete
- [ ] Handle timeouts/missing peers

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