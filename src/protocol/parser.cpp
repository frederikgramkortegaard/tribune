

#include "events/events.hpp"
#include "utils/logging.hpp"
#include <httplib.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>

std::optional<EventResponse> parseSubmitResponse(const std::string &body) {
  try {
    DEBUG_DEBUG("Parsing SubmitResponse");
    nlohmann::json j = nlohmann::json::parse(body);
    
    // Validate Required Fields
    if (!j.contains("event_id") || !j.contains("data") ||
        !j.contains("timestamp") || !j.contains("client_id")) {
      DEBUG_DEBUG("Missing required fields in submit request");
      return std::nullopt;
    }
    
    // Set type field if not present
    if (!j.contains("type")) {
      j["type"] = ResponseType::DataPart;
    }
    
    // Use automatic conversion
    return j.get<EventResponse>();
    
  } catch (const nlohmann::json::exception& e) {
    DEBUG_ERROR("JSON parsing error: " << e.what());
    return std::nullopt;
  }
}
std::optional<ConnectResponse> parseConnectResponse(const std::string &body) {
  try {
    DEBUG_DEBUG("Parsing ConnectResponse");
    nlohmann::json j = nlohmann::json::parse(body);
    
    // Validate required fields
    if (!j.contains("client_host") || !j.contains("client_port") || 
        !j.contains("client_id") || !j.contains("ed25519_pub")) {
      DEBUG_DEBUG("Missing required fields in connect request");
      return std::nullopt;
    }
    
    // Set type field if not present
    if (!j.contains("type")) {
      j["type"] = ResponseType::ConnectionRequest;
    }
    
    // Use automatic conversion
    return j.get<ConnectResponse>();
    
  } catch (const nlohmann::json::exception& e) {
    DEBUG_ERROR("JSON parsing error: " << e.what());
    return std::nullopt;
  }
}
