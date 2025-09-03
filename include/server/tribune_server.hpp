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

class TribuneServer {
public:
  TribuneServer(const std::string &host, int port, const ServerConfig& config = DEFAULT_SERVER_CONFIG);

  void start();
  void announceEvent(const Event& event);
  
  // Event creation with participant selection
  std::optional<Event> createEvent(EventType type, const std::string& event_id, const std::string& computation_type = "sum");
  
  // MPC computation management
  void registerComputation(const std::string& type, std::unique_ptr<MPCComputation> computation);

private:
  // Participant selection (internal)
  std::vector<ClientInfo> selectParticipants();
  // Configuration
  ServerConfig config_;
  
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
  
  // Private methods
  void checkForCompleteResults();
};
