#pragma once
#include <cstdint>
#include <string>
#include <string_view>

namespace tribune {

// Error categories with distinct ranges
enum class ErrorCategory : uint16_t {
    None = 0,
    Network = 1000,
    Crypto = 2000,
    Protocol = 3000,
    MPC = 4000,
    System = 5000
};

// Structured error codes
enum class ErrorCode : uint32_t {
    // Success
    Success = 0,
    
    // Network errors (1000-1999)
    NetworkConnectionFailed = 1001,
    NetworkTimeout = 1002,
    NetworkInvalidResponse = 1003,
    NetworkPeerUnreachable = 1004,
    NetworkBindFailed = 1005,
    
    // Crypto errors (2000-2999)
    CryptoInvalidSignature = 2001,
    CryptoInvalidPublicKey = 2002,
    CryptoInvalidPrivateKey = 2003,
    CryptoSignatureFailed = 2004,
    CryptoKeyGenerationFailed = 2005,
    CryptoInitializationFailed = 2006,
    
    // Protocol errors (3000-3999)
    ProtocolInvalidMessage = 3001,
    ProtocolInvalidEvent = 3002,
    ProtocolClientNotConnected = 3003,
    ProtocolEventTimeout = 3004,
    ProtocolDuplicateEvent = 3005,
    ProtocolMissingShards = 3006,
    
    // MPC errors (4000-4999)
    MPCComputationNotFound = 4001,
    MPCInvalidData = 4002,
    MPCInsufficientParticipants = 4003,
    MPCComputationFailed = 4004,
    
    // System errors (5000-5999)
    SystemResourceExhausted = 5001,
    SystemInvalidConfiguration = 5002,
    SystemThreadCreationFailed = 5003
};

// Result type for operations that can fail
template<typename T>
class Result {
public:
    Result(T value) noexcept : value_(std::move(value)), code_(ErrorCode::Success) {}
    Result(ErrorCode code) noexcept : code_(code) {}
    Result(ErrorCode code, std::string_view message) 
        : code_(code), message_(message) {}
    
    bool isSuccess() const noexcept { return code_ == ErrorCode::Success; }
    bool isError() const noexcept { return code_ != ErrorCode::Success; }
    
    const T& value() const { return value_; }
    T& value() { return value_; }
    T&& moveValue() noexcept { return std::move(value_); }
    
    ErrorCode error() const noexcept { return code_; }
    std::string_view message() const noexcept { return message_; }
    
    operator bool() const noexcept { return isSuccess(); }
    
private:
    T value_;
    ErrorCode code_;
    std::string message_;
};

// Specialization for void results
template<>
class Result<void> {
public:
    Result() noexcept : code_(ErrorCode::Success) {}
    Result(ErrorCode code) noexcept : code_(code) {}
    Result(ErrorCode code, std::string_view message) 
        : code_(code), message_(message) {}
    
    bool isSuccess() const noexcept { return code_ == ErrorCode::Success; }
    bool isError() const noexcept { return code_ != ErrorCode::Success; }
    
    ErrorCode error() const noexcept { return code_; }
    std::string_view message() const noexcept { return message_; }
    
    operator bool() const noexcept { return isSuccess(); }
    
private:
    ErrorCode code_;
    std::string message_;
};

// Helper function to get error category
inline ErrorCategory getErrorCategory(ErrorCode code) noexcept {
    uint32_t value = static_cast<uint32_t>(code);
    if (value == 0) return ErrorCategory::None;
    if (value >= 1000 && value < 2000) return ErrorCategory::Network;
    if (value >= 2000 && value < 3000) return ErrorCategory::Crypto;
    if (value >= 3000 && value < 4000) return ErrorCategory::Protocol;
    if (value >= 4000 && value < 5000) return ErrorCategory::MPC;
    if (value >= 5000 && value < 6000) return ErrorCategory::System;
    return ErrorCategory::None;
}

// Convert error code to string
inline std::string_view errorToString(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Success: return "Success";
        
        // Network errors
        case ErrorCode::NetworkConnectionFailed: return "Network connection failed";
        case ErrorCode::NetworkTimeout: return "Network timeout";
        case ErrorCode::NetworkInvalidResponse: return "Invalid network response";
        case ErrorCode::NetworkPeerUnreachable: return "Peer unreachable";
        case ErrorCode::NetworkBindFailed: return "Failed to bind to address";
        
        // Crypto errors
        case ErrorCode::CryptoInvalidSignature: return "Invalid signature";
        case ErrorCode::CryptoInvalidPublicKey: return "Invalid public key";
        case ErrorCode::CryptoInvalidPrivateKey: return "Invalid private key";
        case ErrorCode::CryptoSignatureFailed: return "Signature creation failed";
        case ErrorCode::CryptoKeyGenerationFailed: return "Key generation failed";
        case ErrorCode::CryptoInitializationFailed: return "Crypto initialization failed";
        
        // Protocol errors
        case ErrorCode::ProtocolInvalidMessage: return "Invalid protocol message";
        case ErrorCode::ProtocolInvalidEvent: return "Invalid event";
        case ErrorCode::ProtocolClientNotConnected: return "Client not connected";
        case ErrorCode::ProtocolEventTimeout: return "Event timed out";
        case ErrorCode::ProtocolDuplicateEvent: return "Duplicate event";
        case ErrorCode::ProtocolMissingShards: return "Missing data shards";
        
        // MPC errors
        case ErrorCode::MPCComputationNotFound: return "Computation type not found";
        case ErrorCode::MPCInvalidData: return "Invalid MPC data";
        case ErrorCode::MPCInsufficientParticipants: return "Insufficient participants";
        case ErrorCode::MPCComputationFailed: return "Computation failed";
        
        // System errors
        case ErrorCode::SystemResourceExhausted: return "System resources exhausted";
        case ErrorCode::SystemInvalidConfiguration: return "Invalid configuration";
        case ErrorCode::SystemThreadCreationFailed: return "Thread creation failed";
        
        default: return "Unknown error";
    }
}

} // namespace tribune