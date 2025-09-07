#pragma once
#include <string>
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

struct ServerConfig {
  // Network settings
  std::string host;
  int port;

  // Participant selection
  int min_participants;
  int max_participants;

  // Event timing
  int event_announce_interval_seconds;
  int event_timeout_boundary;
  
  // Heartbeat settings
  int ping_interval_seconds;
  int client_timeout_seconds;
  
  ServerConfig(const std::string& configFile = "server.json") {
    // Set defaults
    host = "localhost";
    port = 8080;
    min_participants = 3;
    max_participants = 10;
    event_announce_interval_seconds = 40;
    event_timeout_boundary = 120;
    ping_interval_seconds = 10;
    client_timeout_seconds = 30;
    
    // Try to load from config file
    std::ifstream file(configFile);
    if (file.is_open()) {
      try {
        nlohmann::json config;
        file >> config;
        
        if (config.contains("host")) host = config["host"];
        if (config.contains("port")) port = config["port"];
        if (config.contains("min_participants")) min_participants = config["min_participants"];
        if (config.contains("max_participants")) max_participants = config["max_participants"];
        if (config.contains("event_announce_interval_seconds")) event_announce_interval_seconds = config["event_announce_interval_seconds"];
        if (config.contains("event_timeout_boundary")) event_timeout_boundary = config["event_timeout_boundary"];
        if (config.contains("ping_interval_seconds")) ping_interval_seconds = config["ping_interval_seconds"];
        if (config.contains("client_timeout_seconds")) client_timeout_seconds = config["client_timeout_seconds"];
        
        validate();
      } catch (const std::exception& e) {
        throw std::runtime_error("Failed to load config from " + configFile + ": " + e.what());
      }
    } else {
      validate();
    }
  }
  
private:
  void validate() {
    if (port < 1 || port > 65535) {
      throw std::invalid_argument("Invalid port: " + std::to_string(port) + ". Must be 1-65535");
    }
    
    if (min_participants < 1) {
      throw std::invalid_argument("Invalid min_participants: " + std::to_string(min_participants) + ". Must be >= 1");
    }
    
    if (max_participants < min_participants) {
      throw std::invalid_argument("Invalid max_participants: " + std::to_string(max_participants) + ". Must be >= min_participants (" + std::to_string(min_participants) + ")");
    }
    
    if (event_announce_interval_seconds < 1) {
      throw std::invalid_argument("Invalid event_announce_interval_seconds: " + std::to_string(event_announce_interval_seconds) + ". Must be >= 1");
    }
    
    if (event_timeout_boundary < 1) {
      throw std::invalid_argument("Invalid event_timeout_boundary: " + std::to_string(event_timeout_boundary) + ". Must be >= 1");
    }
    
    if (ping_interval_seconds < 1) {
      throw std::invalid_argument("Invalid ping_interval_seconds: " + std::to_string(ping_interval_seconds) + ". Must be >= 1");
    }
    
    if (client_timeout_seconds < ping_interval_seconds) {
      throw std::invalid_argument("Invalid client_timeout_seconds: " + std::to_string(client_timeout_seconds) + ". Must be >= ping_interval_seconds (" + std::to_string(ping_interval_seconds) + ")");
    }
    
    if (host.empty()) {
      throw std::invalid_argument("Host cannot be empty");
    }
  }
};

// Default config
static const ServerConfig DEFAULT_SERVER_CONFIG{};
