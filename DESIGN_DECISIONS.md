# Design Decisions

This document explains key architectural and security decisions made in Tribune.

## Security Architecture

### Cryptographic Approach

Tribune uses Ed25519 digital signatures for peer authentication. We deliberately chose NOT to implement peer-to-peer encryption and removed all X25519 key exchange functionality.

### Inter-Peer Data Security

Data shards shared between peers are **signed** but **NOT encrypted**. Each peer verifies sender authenticity using Ed25519 public keys. Transport security relies on HTTPS/TLS at the network layer.

**Rationale:**
- Individual MPC shards reveal no information about the underlying secret due to secret sharing properties
- Avoiding peer-to-peer encryption reduces complexity and latency  
- Honest-but-curious adversary model assumes participants follow protocol but may try to learn information
- HTTPS provides transport-level encryption against network eavesdropping

### Authentication Flow

```
1. Client registers with server, provides Ed25519 public key
2. Server includes public keys in event participant lists
3. When sharing data:
   - Sender creates signature: sign(event_id|client_id|data, private_key)
   - Receiver verifies: verify(signature, sender_public_key)
4. Only authenticated shards from authorized participants are stored
```

### Security Guarantees

**Protects against:**
- Message tampering (signature verification)
- Impersonation attacks (public key authentication)
- Unauthorized participants injecting data
- Network eavesdropping (HTTPS)

**Does NOT protect against:**
- Malicious server reading plaintext shards
- Peer-to-peer data interception at application layer
- Compromised client keys

### Alternative Approaches Considered

**Peer-to-Peer Encryption** - Could use X25519 key exchange + symmetric encryption. Rejected due to added complexity without significant security benefit given MPC security properties.

**RSA Public Key Encryption** - Could encrypt directly to recipient's public key. Rejected due to performance limitations and message size restrictions.

## Threading and Concurrency

All shared data structures are protected by mutexes with separate mutexes for different concerns (roster, event_shards, orphan_shards, etc.). Atomic operations handle simple flags.

**Orphan Shard Management** uses FIFO queue with size limits to prevent memory exhaustion and handles race conditions where peer data arrives before event announcement.

## Computation Framework

Abstract `MPCComputation` interface allows custom computation types. Apps register computation handlers for different operations (sum, average, ML training, etc.). Events specify computation_type for flexible protocol support.

## Network Architecture

**Star Topology with P2P Data Sharing** - Central server coordinates events and manages participant selection. Clients share data directly with each other. Server never sees individual data shards, only aggregated results.

**Trade-offs:**
- Server provides coordination and discovery
- P2P data sharing scales better than hub-and-spoke  
- Single point of failure for coordination
- Requires trust in server for honest participant selection

## Future Improvements

**Cryptographic**: Replace dummy signatures with real libsodium Ed25519 implementation. Add threshold signatures for decentralized coordination.

**Network**: Implement gossip protocols for serverless coordination. Add Byzantine fault tolerance for malicious participants.

**MPC Protocol**: Implement proper secret sharing schemes (Shamir's Secret Sharing). Add homomorphic encryption for sensitive computations.