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
    
    // Peer management
    int max_events_without_response = 5;  // For isClientParticipating()
};

// Default config
static const ServerConfig DEFAULT_SERVER_CONFIG;