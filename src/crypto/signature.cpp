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
    // For dummy implementation, just check if signature starts with expected prefix
    // In a real implementation, this would use the public key to verify the signature
    
    std::cout << "Verifying dummy signature: ";
    
    if (signature.find("dummy_sig_") == 0) {
        std::cout << "VALID (dummy signature format)" << std::endl;
        return true;
    } else {
        std::cout << "INVALID (not dummy signature format)" << std::endl;
        return false;
    }
}