#pragma once
#include "events/events.hpp"
#include <atomic>
#include <httplib.h>
#include <string>
#include <thread>

class TribuneClient {
public:
  TribuneClient(const std::string &seed_host, int seed_port, int listen_port);
  ~TribuneClient();

  // Core functionality
  bool connectToSeed();
  void startListening();
  void stop();

  // Event handling
  void onEventAnnouncement(const Event &event);

  // Getters
  const std::string &getClientId() const { return client_id_; }
  int getListenPort() const { return listen_port_; }

private:
  // Client identification
  std::string client_id_;
  std::string generateUUID();

  // Network configuration
  std::string seed_host_;
  int seed_port_;
  int listen_port_;

  // Event listener
  std::thread listener_thread_;
  std::atomic<bool> running_;
  httplib::Server event_server_;

  // Peers Management
  std::mutex peers_mutex_;
  std::thread peer_sync_thread_;
  std::vector<ClientInfo> current_peers_;

  // Private methods
  // // Meta
  void runEventListener();
  void setupEventRoutes();
  // // Data Sharding / Peer Relay'ing
};
