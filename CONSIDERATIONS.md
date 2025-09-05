# Architectural Considerations

## Current Design Choices
- Server selecting participant groups instead of decentralized organization
- Not encrypting actual data sent (not needed since it's sharded but still a consideration)
- Star topology vs mesh topology
- Peer propagated events (to avoid orphans), payload data, security, signatures etc.

## Cryptographic Limitations

### Current: Additive Secret Sharing
Tribune currently uses simple additive secret sharing where a value is split into random shards that sum to the original. This works well for:
- Sum operations
- Averages
- Simple aggregations

Limitations:
- Cannot multiply sharded values without revealing information
- Cannot preserve relationships between multiple values (x,y pairs)
- Cannot perform complex operations like regression directly on shards

### Future: Homomorphic Encryption
Homomorphic encryption would allow computation on encrypted data without decrypting it first.

**Partially Homomorphic:**
- Additive homomorphic (Paillier): Can add encrypted values
- Multiplicative homomorphic (RSA): Can multiply encrypted values

**Fully Homomorphic (FHE):**
- Can perform arbitrary computations on encrypted data
- Examples: CKKS (Cheon-Kim-Kim-Song), BGV, BFV schemes
- Trade-offs: Much slower, larger ciphertexts

**CKKS Scheme Specifically:**
- Designed for approximate arithmetic (perfect for ML)
- Supports real/complex numbers (not just integers)
- Enables operations on encrypted floating-point values
- Used in practice for privacy-preserving machine learning
- Trade-off: Results are approximate, not exact

**Use cases that would benefit:**
- Linear regression (need to multiply x*y while encrypted)
- Neural network training
- Statistical operations beyond simple sums

**Implementation considerations:**
- Libraries: SEAL, HElib, TFHE
- Performance overhead: 1000-10000x slower than plaintext
- Ciphertext expansion: 10-100x larger than plaintext
- Noise growth: Need bootstrapping for deep circuits

### Hybrid Approach
Could combine secret sharing for simple operations with homomorphic encryption for complex ones:
1. Use additive sharing for data collection
2. Convert to homomorphic encryption for multiplication
3. Convert back to shares for aggregation

This would enable more complex MPC protocols while keeping the simple cases efficient.