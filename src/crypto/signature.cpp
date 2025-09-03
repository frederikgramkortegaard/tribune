#include "crypto/signature.hpp"
#include <iostream>

std::string SignatureUtils::createMessage(const std::string& event_id, const std::string& from_client, const std::string& data) {
    return event_id + "|" + from_client + "|" + data;
}

std::string SignatureUtils::createSignature(const std::string& message, const std::string& private_key) {
    // TODO: Replace with real Ed25519 signature using libsodium
    // For now, return a dummy signature based on message hash
    std::hash<std::string> hasher;
    size_t hash = hasher(message + private_key);
    
    std::cout << "Creating dummy signature for message: " << message.substr(0, 50) << "..." << std::endl;
    return "dummy_sig_" + std::to_string(hash);
}

bool SignatureUtils::verifySignature(const std::string& message, const std::string& signature, const std::string& public_key) {
    // TODO: Replace with real Ed25519 verification using libsodium
    // For now, recreate the dummy signature and compare
    std::hash<std::string> hasher;
    size_t expected_hash = hasher(message + public_key); // Note: using public key as if it were private for dummy
    
    std::string expected_signature = "dummy_sig_" + std::to_string(expected_hash);
    bool valid = (signature == expected_signature);
    
    std::cout << "Verifying dummy signature: " << (valid ? "VALID" : "INVALID") << std::endl;
    return valid;
}