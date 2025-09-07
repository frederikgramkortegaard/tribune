#pragma once
#include <string>

enum ClientConnectionState {
  CONNECTED,
  DISCONNECTED,
};

class ClientState {

public:
  ClientState() noexcept = default;
  ClientState(const std::string &host, const std::string &port,
              const std::string &id, const std::string &ed25519)
      : client_host_(host), client_port_(port), client_id_(id),
        ed25519_pub_(ed25519) {}
  
  ClientState(ClientState&&) noexcept = default;
  ClientState& operator=(ClientState&&) noexcept = default;
  ClientState(const ClientState&) = default;
  ClientState& operator=(const ClientState&) = default;
  std::string client_host_;
  std::string client_port_;
  std::string client_id_;
  std::string ed25519_pub_;
};

