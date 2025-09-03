#pragma once
#include "events/events.hpp"
#include "data_collection_module.hpp"
#include <atomic>
#include <httplib.h>
#include <string>
#include <thread>
#include <memory>

class TribuneClient {
public:
  TribuneClient(const std::string &seed_host, int seed_port, int listen_port);
  ~TribuneClient();

  // Core functionality
  bool connectToSeed();
  void startListening();
  void stop();
  
  // Data collection module management
  void setDataCollectionModule(std::unique_ptr<DataCollectionModule> module);

  // Event handling
  void onEventAnnouncement(const Event &event);
  void onPeerDataReceived(const PeerDataMessage &peer_msg);
  
  // Peer coordination
  void shareDataWithPeers(const Event &event, const std::string &my_data);

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
  
  // Data collection
  std::unique_ptr<DataCollectionModule> data_module_;

  // Private methods
  void runEventListener();
  void setupEventRoutes();
};
