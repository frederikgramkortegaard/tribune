#pragma once
#include <string>

struct ServerConfig {
  // Network settings
  std::string host = "localhost";
  int port = 8080;

  // Participant selection
  int min_participants = 3;
  int max_participants = 10;

  // Event timing
  int event_announce_interval_seconds = 40;
  int event_timeout_boundary = 120; // How many seconds an event has needed to
                                    // be active before we time it out
};

// Default config
static const ServerConfig DEFAULT_SERVER_CONFIG = {
    "localhost", // host
    8080,        // port
    3,           // min_participants
    10,          // max_participants
    40           // event_announce_interval_seconds
};
