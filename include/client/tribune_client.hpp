#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <httplib.h>
#include "events/events.hpp"

class TribuneClient {
public:
    TribuneClient(const std::string& seed_host, int seed_port, int listen_port);
    ~TribuneClient();
    
    // Core functionality
    bool connectToSeed();
    void startListening();
    void stop();
    
    // Event handling
    void onEventAnnouncement(const Event& event);
    
    // Getters
    const std::string& getClientId() const { return client_id_; }
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
    
    // Private methods
    void runEventListener();
    void setupEventRoutes();
};