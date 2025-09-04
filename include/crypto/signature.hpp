#pragma once
#include <string>
#include <utility>

class SignatureUtils {
public:
    // Ed25519 signature operations using libsodium
    static std::string createSignature(const std::string& message, const std::string& private_key);
    static bool verifySignature(const std::string& message, const std::string& signature, const std::string& public_key);
    
    // Key generation
    static std::pair<std::string, std::string> generateKeyPair(); // Returns (public_key, private_key)
    
private:
    static std::string createMessage(const std::string& event_id, const std::string& from_client, const std::string& data);
};