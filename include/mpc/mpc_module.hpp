#pragma once
#include "events/events.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <nlohmann/json.hpp>

// Forward declaration
struct Event;

// Represents a shard of data for a participant
struct DataShard {
    std::string participant_id;
    std::string data;  // Can be encrypted or plaintext depending on protocol
    int shard_index;
    nlohmann::json metadata;  // Protocol-specific metadata
};

// Result from a single participant's computation
struct PartialResult {
    std::string participant_id;
    nlohmann::json value;  // Computation result
    std::string proof;     // Optional proof of correct computation
    std::string signature;  // Signature over the result
};

// Final aggregated result
struct FinalResult {
    nlohmann::json value;
    std::string combined_signature;  // Optional threshold/combined signature
    bool verified = false;
};

// Metadata about the MPC protocol
struct ProtocolMetadata {
    std::string protocol_name;
    int min_participants;
    int threshold;  // For threshold protocols
    bool requires_trusted_setup;
    nlohmann::json parameters;  // Protocol-specific parameters
};

// Abstract base class for MPC modules
// Handles the complete MPC protocol lifecycle:
// sharding -> masking -> computation -> aggregation -> verification
class MPCModule {
public:
    virtual ~MPCModule() = default;
    
    // ===== Data Preparation Phase =====
    
    // Shard input data among participants
    // Event provides participant list and public keys
    virtual std::vector<DataShard> shardData(const std::string& raw_data,
                                            const Event* event) = 0;
    
    // Apply masking/randomization to shards
    // Event provides keys for encryption/masking
    virtual std::vector<DataShard> maskShards(const std::vector<DataShard>& shards,
                                             const Event* event,
                                             const std::string& participant_id) = 0;
    
    // ===== Computation Phase =====
    
    // Perform partial computation on collected shards from all participants
    // Each participant collects shards at the same index position from all other participants
    // Event provides context for verification
    virtual PartialResult computePartial(const Event* event,
                                        const std::vector<DataShard>& collected_shards) = 0;
    
    // ===== Aggregation Phase =====
    
    // Aggregate partial results into final result
    // Event provides public keys for signature verification
    virtual FinalResult aggregate(const std::vector<PartialResult>& partials,
                                const Event* event) = 0;
    
    // ===== Verification Phase =====
    
    // Verify the correctness of a final result
    // Event provides keys and threshold parameters
    virtual bool verifyResult(const FinalResult& result,
                            const Event* event) = 0;
    
    // ===== Protocol Management =====
    
    // Check if the protocol has completed for an event
    virtual bool isProtocolComplete(const std::string& event_id) const = 0;
    
    // Reset protocol state for an event
    virtual void reset(const std::string& event_id) = 0;
    
    // Get protocol metadata and requirements
    virtual ProtocolMetadata getProtocolMetadata() const = 0;
};

// Placeholder for actual MPC module implementations
// This will be replaced with real implementations like:
// - SecureSumModule
// - SecureAverageModule  
// - ThresholdSignatureModule
// - PrivateSetIntersectionModule
// - SecureMultipartyAuctionModule
// etc.