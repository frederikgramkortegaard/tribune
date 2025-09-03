

#include "events/events.hpp"
#include <httplib.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>

std::optional<EventResponse> parseSubmitResponse(const std::string &body) {
  try {
    std::cout << "Parsing SubmitResponse" << std::endl;
    nlohmann::json j = nlohmann::json::parse(body);
    
    // Validate Required Fields
    if (!j.contains("event_id") || !j.contains("data") ||
        !j.contains("timestamp") || !j.contains("client_id")) {
      std::cout << "Missing required fields in submit request" << std::endl;
      return std::nullopt;
    }
    
    // Set type field if not present
    if (!j.contains("type")) {
      j["type"] = ResponseType::DataPart;
    }
    
    // Use automatic conversion
    return j.get<EventResponse>();
    
  } catch (const nlohmann::json::exception& e) {
    std::cout << "JSON parsing error: " << e.what() << std::endl;
    return std::nullopt;
  }
}
std::optional<ConnectResponse> parseConnectResponse(const std::string &body) {
  try {
    std::cout << "Parsing ConnectResponse" << std::endl;
    nlohmann::json j = nlohmann::json::parse(body);
    
    // Validate required fields
    if (!j.contains("client_host") || !j.contains("client_port") || 
        !j.contains("client_id") || !j.contains("x25519_pub") || 
        !j.contains("ed25519_pub")) {
      std::cout << "Missing required fields in connect request" << std::endl;
      return std::nullopt;
    }
    
    // Set type field if not present
    if (!j.contains("type")) {
      j["type"] = ResponseType::ConnectionRequest;
    }
    
    // Use automatic conversion
    return j.get<ConnectResponse>();
    
  } catch (const nlohmann::json::exception& e) {
    std::cout << "JSON parsing error: " << e.what() << std::endl;
    return std::nullopt;
  }
}
