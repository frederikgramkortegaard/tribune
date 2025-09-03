#pragma once
#include "client_state.hpp"
#include "events/events.hpp"
#include <httplib.h>
#include <string>
#include <unordered_map>

class TribuneServer {
public:
  TribuneServer(const std::string &host, int port);

  void start();
  void announceEvent(const Event);

private:
  // Meta
  httplib::Server svr;
  std::string host;
  int port;

  // Transport Layer Management
  std::unordered_map<std::string, ClientState> roster;

  // Private Methods
  void setupRoutes();
  void handleEndpointSubmit(const httplib::Request &, httplib::Response &);
  void handleEndpointConnect(const httplib::Request &, httplib::Response &);
  void handleEndpointPeers(const httplib::Request &, httplib::Response &);

  // Event/Response Aggregation/Processing
  std::unordered_map<std::string,
                     std::unordered_map<std::string, EventResponse>>
      unprocessed_responses;
  std::queue<Event> pending_events;
  std::mutex event_mutex;
};
