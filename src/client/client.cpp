#include "client/client.hpp"
#include "protocol/parser.hpp"
#include <iostream>
#include <random>
#include <sstream>
#include <iomanip>

TribuneClient::TribuneClient(const std::string& seed_host, int seed_port, int listen_port)
    : seed_host_(seed_host), seed_port_(seed_port), listen_port_(listen_port), running_(false) {
    
    client_id_ = generateUUID();
    setupEventRoutes();
    
    std::cout << "Created TribuneClient with ID: " << client_id_ << std::endl;
    std::cout << "Will connect to seed: " << seed_host_ << ":" << seed_port_ << std::endl;
    std::cout << "Listening on port: " << listen_port_ << std::endl;
}

TribuneClient::~TribuneClient() {
    stop();
}

std::string TribuneClient::generateUUID() {
    // Simple UUID v4 generation using random numbers
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    
    for (int i = 0; i < 32; ++i) {
        if (i == 8 || i == 12 || i == 16 || i == 20) {
            ss << "-";
        }
        ss << dis(gen);
    }
    
    return "client-" + ss.str();
}

bool TribuneClient::connectToSeed() {
    try {
        httplib::Client cli(seed_host_, seed_port_);
        
        // Create connect request
        ConnectResponse connect_req;
        connect_req.type_ = ResponseType::ConnectionRequest;
        connect_req.client_host = "localhost";  // TODO: Get actual IP
        connect_req.client_port = std::to_string(listen_port_);
        connect_req.client_id = client_id_;
        connect_req.x25519_pub = "dummy_x25519_key";  // TODO: Generate real keys
        connect_req.ed25519_pub = "dummy_ed25519_key"; // TODO: Generate real keys
        
        // Convert to JSON
        nlohmann::json j = connect_req;
        std::string json_body = j.dump();
        
        std::cout << "Connecting to seed node..." << std::endl;
        auto res = cli.Post("/connect", json_body, "application/json");
        
        if (res && res->status == 200) {
            std::cout << "Successfully connected to seed node!" << std::endl;
            std::cout << "Response: " << res->body << std::endl;
            return true;
        } else {
            std::cout << "Failed to connect to seed node. Status: " 
                      << (res ? std::to_string(res->status) : "No response") << std::endl;
            if (res) {
                std::cout << "Response body: " << res->body << std::endl;
            }
            return false;
        }
        
    } catch (const std::exception& e) {
        std::cout << "Exception during connection: " << e.what() << std::endl;
        return false;
    }
}

void TribuneClient::setupEventRoutes() {
    // Setup the /event endpoint to receive announcements
    event_server_.Post("/event", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            std::cout << "Received event announcement: " << req.body << std::endl;
            
            // Parse the event
            nlohmann::json j = nlohmann::json::parse(req.body);
            Event event = j.get<Event>();
            
            // Handle the event
            onEventAnnouncement(event);
            
            // Send response
            res.status = 200;
            res.set_content("{\"status\":\"received\"}", "application/json");
            
        } catch (const std::exception& e) {
            std::cout << "Error processing event: " << e.what() << std::endl;
            res.status = 400;
            res.set_content("{\"error\":\"Failed to process event\"}", "application/json");
        }
    });
}

void TribuneClient::startListening() {
    running_ = true;
    listener_thread_ = std::thread(&TribuneClient::runEventListener, this);
    std::cout << "Started event listener on port " << listen_port_ << std::endl;
}

void TribuneClient::runEventListener() {
    std::cout << "Event listener thread started" << std::endl;
    event_server_.listen("localhost", listen_port_);
}

void TribuneClient::onEventAnnouncement(const Event& event) {
    std::cout << "=== EVENT RECEIVED ===" << std::endl;
    std::cout << "Event ID: " << event.event_id << std::endl;
    std::cout << "Event Type: " << event.type_ << std::endl;
    std::cout << "======================" << std::endl;
    
    // TODO: This is where data collection modules would be called
    // For now, just acknowledge receipt
}

void TribuneClient::stop() {
    if (running_) {
        running_ = false;
        event_server_.stop();
        
        if (listener_thread_.joinable()) {
            listener_thread_.join();
        }
        
        std::cout << "Client stopped" << std::endl;
    }
}