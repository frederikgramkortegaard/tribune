#pragma once
#include "client_state.hpp"
#include "events/events.hpp"
#include "mpc/mpc_computation.hpp"
#include "server_config.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <httplib.h>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <random>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

class TribuneServer {
public:
  TribuneServer(const std::string &host, int port,
                const ServerConfig &config = DEFAULT_SERVER_CONFIG);
  ~TribuneServer();

  void start();
  void stop();
  void announceEvent(const Event &event, std::string *result = nullptr);

  // Event creation with participant selection
  std::optional<Event> createEvent(EventType type, const std::string &event_id,
                                   const std::string &computation_type = "sum");

  // MPC computation management
  void registerComputation(const std::string &type,
                           std::unique_ptr<MPCComputation> computation);

  // Server identity
  const std::string &getServerPublicKey() const { return server_public_key_; }

private:
  // Participant selection (internal)
  std::vector<ClientInfo> selectParticipants();
  // Configuration
  ServerConfig config_;

  // Server cryptographic identity
  std::string server_private_key_;
  std::string server_public_key_;

  // Meta
  httplib::Server svr_;
  std::string host_;
  int port_;

  // Random number generation
  std::random_device rd_;
  std::mt19937 rng_;
  std::mutex rng_mutex_;

  // Transport Layer Management (read-heavy: peer queries, participant selection)
  std::unordered_map<std::string, ClientState> roster_;
  std::shared_mutex roster_mutex_;

  // Private Methods
  void setupRoutes();
  void handleEndpointSubmit(const httplib::Request &, httplib::Response &);
  void handleEndpointConnect(const httplib::Request &, httplib::Response &);
  void handleEndpointPeers(const httplib::Request &, httplib::Response &);

  // Event/Response Aggregation/Processing
  // Read-heavy: multiple threads checking completion status
  std::unordered_map<std::string,
                     std::unordered_map<std::string, EventResponse>>
      unprocessed_responses_;
  std::shared_mutex unprocessed_responses_mutex_;
  std::queue<Event> pending_events_;
  std::mutex event_mutex_;

  // MPC computations (read-heavy: lookups during event processing)
  std::unordered_map<std::string, std::unique_ptr<MPCComputation>>
      computations_;
  std::shared_mutex computations_mutex_;

  // Active event tracking (read-heavy: status checks, completion monitoring)
  struct ActiveEvent {
    std::string event_id;
    std::string computation_type;
    int expected_participants;
    std::chrono::time_point<std::chrono::steady_clock> created_time;
    std::string *result_ptr = nullptr; // Optional pointer to store final result
    const Event event;
  };
  std::unordered_map<std::string, ActiveEvent> active_events_;
  std::shared_mutex active_events_mutex_;

  // Private methods
  void checkForCompleteResults();
  void periodicEventChecker();

  // Background thread for periodic checking
  std::thread checker_thread_;
  std::atomic<bool> should_stop_{false};
};
