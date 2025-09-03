#pragma once
#include <string>

class SignatureUtils {
public:
    // TODO: Replace with real Ed25519 implementation using libsodium or similar
    static std::string createSignature(const std::string& message, const std::string& private_key);
    static bool verifySignature(const std::string& message, const std::string& signature, const std::string& public_key);
    
private:
    static std::string createMessage(const std::string& event_id, const std::string& from_client, const std::string& data);
};