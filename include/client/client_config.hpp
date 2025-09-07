#pragma once
#include <string>
#include <fstream>
#include <stdexcept>
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
  
  // TLS settings
  bool use_tls;
  bool verify_server_cert;
  
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
    use_tls = false;
    verify_server_cert = true;
    
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
        if (config.contains("use_tls")) use_tls = config["use_tls"];
        if (config.contains("verify_server_cert")) verify_server_cert = config["verify_server_cert"];
        
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
    if (server_port < 1 || server_port > 65535) {
      throw std::invalid_argument("Invalid server_port: " + std::to_string(server_port) + ". Must be 1-65535");
    }
    
    if (listen_port < 0 || listen_port > 65535) {
      throw std::invalid_argument("Invalid listen_port: " + std::to_string(listen_port) + ". Must be 0-65535 (0 = auto-assign)");
    }
    
    if (health_check_interval_seconds < 1) {
      throw std::invalid_argument("Invalid health_check_interval_seconds: " + std::to_string(health_check_interval_seconds) + ". Must be >= 1");
    }
    
    if (server_timeout_seconds < health_check_interval_seconds) {
      throw std::invalid_argument("Invalid server_timeout_seconds: " + std::to_string(server_timeout_seconds) + ". Must be >= health_check_interval_seconds (" + std::to_string(health_check_interval_seconds) + ")");
    }
    
    if (connection_timeout_seconds < 1) {
      throw std::invalid_argument("Invalid connection_timeout_seconds: " + std::to_string(connection_timeout_seconds) + ". Must be >= 1");
    }
    
    if (read_timeout_seconds < 1) {
      throw std::invalid_argument("Invalid read_timeout_seconds: " + std::to_string(read_timeout_seconds) + ". Must be >= 1");
    }
    
    if (server_host.empty()) {
      throw std::invalid_argument("Server host cannot be empty");
    }
    
    if (listen_host.empty()) {
      throw std::invalid_argument("Listen host cannot be empty");
    }
  }
};

// Default config
static const ClientConfig DEFAULT_CLIENT_CONFIG{};