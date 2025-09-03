#pragma once
#include <string>

enum ClientConnectionState {
  CONNECTED,
  DISCONNECTED,
};

class ClientState {

public:
  ClientState() : eventsSinceLastClientResponse(0) {}
  ClientState(const std::string &host, const std::string &port,
              const std::string &id, const std::string &ed25519)
      : client_host(host), client_port(port), client_id(id),
        ed25519_pub(ed25519), eventsSinceLastClientResponse(0) {}
  std::string client_host;
  std::string client_port;
  std::string client_id;
  std::string ed25519_pub;

  bool isClientParticipating() const;
  void markReceivedEvent();

private:
  int eventsSinceLastClientResponse = 0;
};

