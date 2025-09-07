#pragma once
#include "crypto/signature.hpp"
#include "data_collection_module.hpp"
#include "events/events.hpp"
#include "mpc/mpc_computation.hpp"
#include <atomic>
#include <chrono>
#include <httplib.h>
#include <memory>
#include <mutex>
#include <queue>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

class TribuneClient {
public:
  TribuneClient(const std::string &seed_host, int seed_port,
                const std::string &listen_host, int listen_port,
                const std::string &private_key = "",
                const std::string &public_key = "");
  ~TribuneClient();

  // Core functionality
  bool connectToSeed();
  void startListening();
  void stop();

  // Data collection module management
  void setDataCollectionModule(std::unique_ptr<DataCollectionModule> module);

  // MPC computation management
  void registerComputation(const std::string &type,
                           std::unique_ptr<MPCComputation> computation);

  // Event handling
  void onEventAnnouncement(const Event &event, bool relay = true);
  void onPeerDataReceived(const PeerDataMessage &peer_msg);

  // Peer coordination
  void shareDataWithPeers(const Event &event, const std::string &my_data);

  // Getters
  const std::string &getClientId() const { return client_id_; }
  int getListenPort() const { return listen_port_; }
  const std::string &getListenHost() const { return listen_host_; }

private:
  // Client identification
  std::string client_id_;
  std::string ed25519_private_key_; // Private key for signing
  std::string ed25519_public_key_;  // Public key for verification
  std::string
      server_public_key_; // Server's public key for signature verification
  std::string generateUUID();

  // Network configuration
  std::string seed_host_;
  int seed_port_;
  std::string listen_host_;
  int listen_port_;

  // Event listener
  std::thread listener_thread_;
  std::atomic<bool> running_;
  httplib::Server event_server_;

  // Active events we're participating in (read-heavy: status checks, data collection)
  std::unordered_map<std::string, Event> active_events_;
  std::shared_mutex active_events_mutex_;

  // Shards storage: <event_id, <client_id, data>> (read-heavy: completion checks)
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      event_shards_;
  std::shared_mutex event_shards_mutex_;

  // Note: Orphan shards are no longer needed with peer event propagation

  // Data collection
  std::unique_ptr<DataCollectionModule> data_module_;
  std::mutex data_module_mutex_;

  // MPC computations (read-heavy: computation lookups)
  std::unordered_map<std::string, std::unique_ptr<MPCComputation>>
      computations_;
  std::shared_mutex computations_mutex_;
  
  // Track events currently being computed to prevent duplicate computation threads
  std::unordered_set<std::string> computing_events_;
  std::mutex computing_events_mutex_;

  // TTL-based deduplication for broadcast storm prevention (read-heavy: duplicate checks)
  struct RecentItem {
    std::chrono::steady_clock::time_point received_time;
  };
  std::unordered_map<std::string, RecentItem> recent_events_;
  std::unordered_map<std::string, RecentItem>
      recent_shards_; // event_id + "|" + client_id
  std::shared_mutex recent_items_mutex_;
  int cleanup_counter_ = 0; // Counter for periodic cleanup
  static constexpr int RECENT_ITEMS_TTL_SECONDS = 60; // 2x event timeout
  static constexpr int EVENT_TIMEOUT_SECONDS = 30;    // Match server timeout
  static constexpr int CLEANUP_FREQUENCY = 50; // Clean up every N peer messages

  // Private methods
  void runEventListener();
  void setupEventRoutes();
  void computeAndSubmitResult(const std::string &event_id);
  std::string runComputation(const std::string &event_id);
  bool submitResult(const std::string &event_id, const std::string &result);
  bool hasAllShards(const std::string &event_id);
  void cleanupRecentItems();
  bool verifyEventFromServer(const Event &event);
};
