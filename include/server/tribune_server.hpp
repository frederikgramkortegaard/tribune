#pragma once
#include "client_state.hpp"
#include "events/events.hpp"
#include "server_config.hpp"
#include "mpc/mpc_computation.hpp"
#include <httplib.h>
#include <string>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <queue>
#include <optional>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>

class TribuneServer {
public:
  TribuneServer(const std::string &host, int port, const ServerConfig& config = DEFAULT_SERVER_CONFIG);
  ~TribuneServer();

  void start();
  void stop();
  void announceEvent(const Event& event);
  
  // Event creation with participant selection
  std::optional<Event> createEvent(EventType type, const std::string& event_id, const std::string& computation_type = "sum");
  
  // MPC computation management
  void registerComputation(const std::string& type, std::unique_ptr<MPCComputation> computation);
  
  // Server identity
  const std::string& getServerPublicKey() const { return server_public_key_; }

private:
  // Participant selection (internal)
  std::vector<ClientInfo> selectParticipants();
  // Configuration
  ServerConfig config_;
  
  // Server cryptographic identity
  std::string server_private_key_;
  std::string server_public_key_;
  
  // Meta
  httplib::Server svr;
  std::string host;
  int port;
  
  // Random number generation
  std::random_device rd_;
  std::mt19937 rng_;
  std::mutex rng_mutex_;

  // Transport Layer Management
  std::unordered_map<std::string, ClientState> roster;
  std::mutex roster_mutex_;

  // Private Methods
  void setupRoutes();
  void handleEndpointSubmit(const httplib::Request &, httplib::Response &);
  void handleEndpointConnect(const httplib::Request &, httplib::Response &);
  void handleEndpointPeers(const httplib::Request &, httplib::Response &);

  // Event/Response Aggregation/Processing
  std::unordered_map<std::string,
                     std::unordered_map<std::string, EventResponse>>
      unprocessed_responses;
  std::mutex unprocessed_responses_mutex_;
  std::queue<Event> pending_events;
  std::mutex event_mutex;
  
  // MPC computations
  std::unordered_map<std::string, std::unique_ptr<MPCComputation>> computations_;
  std::mutex computations_mutex_;
  
  // Active event tracking
  struct ActiveEvent {
    std::string event_id;
    std::string computation_type;
    int expected_participants;
    std::chrono::time_point<std::chrono::steady_clock> created_time;
  };
  std::unordered_map<std::string, ActiveEvent> active_events_;
  std::mutex active_events_mutex_;
  
  // Private methods
  void checkForCompleteResults();
  void periodicEventChecker();
  
  // Background thread for periodic checking
  std::thread checker_thread_;
  std::atomic<bool> should_stop_{false};
};
