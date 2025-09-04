#include "crypto/signature.hpp"
#include "utils/logging.hpp"
#include <iostream>
#include <sodium.h>
#include <vector>
#include <stdexcept>

std::string SignatureUtils::createMessage(const std::string& event_id, const std::string& from_client, const std::string& data) {
    return event_id + "|" + from_client + "|" + data;
}

std::string SignatureUtils::createSignature(const std::string& message, const std::string& private_key) {
    // Initialize libsodium if not already done
    if (sodium_init() < 0) {
        throw std::runtime_error("Failed to initialize libsodium");
    }

    // Convert hex private key to bytes
    if (private_key.length() != crypto_sign_SECRETKEYBYTES * 2) {
        throw std::runtime_error("Invalid private key length");
    }
    
    std::vector<unsigned char> sk_bytes(crypto_sign_SECRETKEYBYTES);
    for (size_t i = 0; i < crypto_sign_SECRETKEYBYTES; ++i) {
        std::string byte_str = private_key.substr(i * 2, 2);
        sk_bytes[i] = static_cast<unsigned char>(std::stoi(byte_str, nullptr, 16));
    }

    // Create signature
    std::vector<unsigned char> signature(crypto_sign_BYTES);
    unsigned long long signature_len;
    
    if (crypto_sign_detached(
        signature.data(), &signature_len,
        reinterpret_cast<const unsigned char*>(message.c_str()), message.length(),
        sk_bytes.data()) != 0) {
        throw std::runtime_error("Failed to create signature");
    }

    // Convert signature to hex string
    std::string hex_signature;
    hex_signature.reserve(crypto_sign_BYTES * 2);
    for (size_t i = 0; i < crypto_sign_BYTES; ++i) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", signature[i]);
        hex_signature += hex;
    }
    
    DEBUG_DEBUG("Created Ed25519 signature for message: " << message.substr(0, 50) << "...");
    return hex_signature;
}

bool SignatureUtils::verifySignature(const std::string& message, const std::string& signature, const std::string& public_key) {
    // Initialize libsodium if not already done
    if (sodium_init() < 0) {
        DEBUG_ERROR("Failed to initialize libsodium for signature verification");
        return false;
    }

    // Validate input lengths
    if (public_key.length() != crypto_sign_PUBLICKEYBYTES * 2) {
        DEBUG_ERROR("Invalid public key length: " << public_key.length());
        return false;
    }
    
    if (signature.length() != crypto_sign_BYTES * 2) {
        DEBUG_ERROR("Invalid signature length: " << signature.length());
        return false;
    }

    try {
        // Convert hex public key to bytes
        std::vector<unsigned char> pk_bytes(crypto_sign_PUBLICKEYBYTES);
        for (size_t i = 0; i < crypto_sign_PUBLICKEYBYTES; ++i) {
            std::string byte_str = public_key.substr(i * 2, 2);
            pk_bytes[i] = static_cast<unsigned char>(std::stoi(byte_str, nullptr, 16));
        }

        // Convert hex signature to bytes
        std::vector<unsigned char> sig_bytes(crypto_sign_BYTES);
        for (size_t i = 0; i < crypto_sign_BYTES; ++i) {
            std::string byte_str = signature.substr(i * 2, 2);
            sig_bytes[i] = static_cast<unsigned char>(std::stoi(byte_str, nullptr, 16));
        }

        // Verify signature
        int result = crypto_sign_verify_detached(
            sig_bytes.data(),
            reinterpret_cast<const unsigned char*>(message.c_str()), message.length(),
            pk_bytes.data());

        if (result == 0) {
            DEBUG_DEBUG("Ed25519 signature VALID for message: " << message.substr(0, 30) << "...");
            return true;
        } else {
            DEBUG_DEBUG("Ed25519 signature INVALID for message: " << message.substr(0, 30) << "...");
            return false;
        }
        
    } catch (const std::exception& e) {
        DEBUG_ERROR("Error during signature verification: " << e.what());
        return false;
    }
}

std::pair<std::string, std::string> SignatureUtils::generateKeyPair() {
    // Initialize libsodium if not already done
    if (sodium_init() < 0) {
        throw std::runtime_error("Failed to initialize libsodium");
    }

    // Generate Ed25519 keypair
    std::vector<unsigned char> pk(crypto_sign_PUBLICKEYBYTES);
    std::vector<unsigned char> sk(crypto_sign_SECRETKEYBYTES);
    
    crypto_sign_keypair(pk.data(), sk.data());
    
    // Convert to hex strings
    std::string public_key_hex;
    std::string private_key_hex;
    
    public_key_hex.reserve(crypto_sign_PUBLICKEYBYTES * 2);
    for (size_t i = 0; i < crypto_sign_PUBLICKEYBYTES; ++i) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", pk[i]);
        public_key_hex += hex;
    }
    
    private_key_hex.reserve(crypto_sign_SECRETKEYBYTES * 2);
    for (size_t i = 0; i < crypto_sign_SECRETKEYBYTES; ++i) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02x", sk[i]);
        private_key_hex += hex;
    }
    
    DEBUG_DEBUG("Generated new Ed25519 keypair");
    
    return std::make_pair(public_key_hex, private_key_hex);
}