#pragma once
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

struct ClientConfig {
  // Server connection
  std::string server_host;
  int server_port;
  
  // Client network settings
  std::string listen_host;
  int listen_port;
  
  // Health monitoring
  int health_check_interval_seconds;
  int server_timeout_seconds;
  
  // Connection settings
  int connection_timeout_seconds;
  int read_timeout_seconds;
  
  ClientConfig(const std::string& configFile = "client.json") {
    // Set defaults
    server_host = "localhost";
    server_port = 8080;
    listen_host = "localhost";
    listen_port = 0; // Auto-assign
    health_check_interval_seconds = 10;
    server_timeout_seconds = 30;
    connection_timeout_seconds = 2;
    read_timeout_seconds = 5;
    
    // Try to load from config file
    std::ifstream file(configFile);
    if (file.is_open()) {
      try {
        nlohmann::json config;
        file >> config;
        
        if (config.contains("server_host")) server_host = config["server_host"];
        if (config.contains("server_port")) server_port = config["server_port"];
        if (config.contains("listen_host")) listen_host = config["listen_host"];
        if (config.contains("listen_port")) listen_port = config["listen_port"];
        if (config.contains("health_check_interval_seconds")) health_check_interval_seconds = config["health_check_interval_seconds"];
        if (config.contains("server_timeout_seconds")) server_timeout_seconds = config["server_timeout_seconds"];
        if (config.contains("connection_timeout_seconds")) connection_timeout_seconds = config["connection_timeout_seconds"];
        if (config.contains("read_timeout_seconds")) read_timeout_seconds = config["read_timeout_seconds"];
      } catch (const std::exception&) {
        // If config file is invalid, just use defaults
      }
    }
  }
};

// Default config
static const ClientConfig DEFAULT_CLIENT_CONFIG{};