#include "client/tribune_client.hpp"
#include "client/data_collection_module.hpp"
#include "protocol/parser.hpp"
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

TribuneClient::TribuneClient(const std::string &seed_host, int seed_port,
                             int listen_port)
    : seed_host_(seed_host), seed_port_(seed_port), listen_port_(listen_port),
      running_(false) {

  client_id_ = generateUUID();
  setupEventRoutes();
  
  // Set default mock data collection module
  data_module_ = std::make_unique<MockDataCollectionModule>(client_id_);

  std::cout << "Created TribuneClient with ID: " << client_id_ << std::endl;
  std::cout << "Will connect to seed: " << seed_host_ << ":" << seed_port_
            << std::endl;
  std::cout << "Listening on port: " << listen_port_ << std::endl;
}

TribuneClient::~TribuneClient() { stop(); }

void TribuneClient::setDataCollectionModule(std::unique_ptr<DataCollectionModule> module) {
  data_module_ = std::move(module);
  std::cout << "Data collection module updated" << std::endl;
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
    connect_req.client_host = "localhost"; // TODO: Get actual IP
    connect_req.client_port = std::to_string(listen_port_);
    connect_req.client_id = client_id_;
    connect_req.x25519_pub = "dummy_x25519_key";   // TODO: Generate real keys
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
                << (res ? std::to_string(res->status) : "No response")
                << std::endl;
      if (res) {
        std::cout << "Response body: " << res->body << std::endl;
      }
      return false;
    }

  } catch (const std::exception &e) {
    std::cout << "Exception during connection: " << e.what() << std::endl;
    return false;
  }
}

void TribuneClient::setupEventRoutes() {
  // Setup the /event endpoint to receive announcements
  event_server_.Post(
      "/event", [this](const httplib::Request &req, httplib::Response &res) {
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

        } catch (const std::exception &e) {
          std::cout << "Error processing event: " << e.what() << std::endl;
          res.status = 400;
          res.set_content("{\"error\":\"Failed to process event\"}",
                          "application/json");
        }
      });

  // Setup the /peer-data endpoint to receive data from other clients
  event_server_.Post("/peer-data", [this](const httplib::Request &req,
                                          httplib::Response &res) {
    try {
      std::cout << "Received peer data: " << req.body << std::endl;

      // Parse the peer data message
      nlohmann::json j = nlohmann::json::parse(req.body);
      PeerDataMessage peer_msg = j.get<PeerDataMessage>();

      // Handle the peer data
      onPeerDataReceived(peer_msg);

      // Send response
      res.status = 200;
      res.set_content("{\"status\":\"received\"}", "application/json");

    } catch (const std::exception &e) {
      std::cout << "Error processing peer data: " << e.what() << std::endl;
      res.status = 400;
      res.set_content("{\"error\":\"Failed to process peer data\"}",
                      "application/json");
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

void TribuneClient::onEventAnnouncement(const Event &event) {
  std::cout << "=== EVENT RECEIVED ===" << std::endl;
  std::cout << "Event ID: " << event.event_id << std::endl;
  std::cout << "Event Type: " << event.type_ << std::endl;
  std::cout << "Participants: " << event.participants.size() << std::endl;
  std::cout << "======================" << std::endl;

  // Use data collection module to get client's data for this event
  std::string my_data;
  if (data_module_) {
    my_data = data_module_->collectData(event);
    std::cout << "Collected data: " << my_data << std::endl;
  } else {
    my_data = "no_data_module_configured";
    std::cout << "Warning: No data collection module configured!" << std::endl;
  }
  
  shareDataWithPeers(event, my_data);
}

void TribuneClient::onPeerDataReceived(const PeerDataMessage &peer_msg) {
  std::cout << "=== PEER DATA RECEIVED ===" << std::endl;
  std::cout << "Event ID: " << peer_msg.event_id << std::endl;
  std::cout << "From Client: " << peer_msg.from_client << std::endl;
  std::cout << "Data: " << peer_msg.data << std::endl;
  std::cout << "===========================" << std::endl;

  // TODO: Process the received peer data
  // - Store it for aggregation
  // - Check if we have data from all expected peers
  // - Perform computation when ready
  // - Eventually send EventResponse back to server
}

void TribuneClient::shareDataWithPeers(const Event &event,
                                       const std::string &my_data) {
  std::cout << "Sharing data with peers for event: " << event.event_id
            << std::endl;

  for (const auto &peer : event.participants) {
    // Skip ourselves
    if (peer.client_id == client_id_) {
      continue;
    }

    std::cout << "Sending data to peer: " << peer.client_id << " at "
              << peer.client_host << ":" << peer.client_port << std::endl;

    // Split data into len(event.participants) - 1 shards

    try {
      httplib::Client cli(peer.client_host, std::stoi(peer.client_port));

      // Create data sharing payload
      nlohmann::json payload;
      payload["event_id"] = event.event_id;
      payload["from_client"] = client_id_;
      payload["data"] = my_data;

      auto res = cli.Post("/peer-data", payload.dump(), "application/json");

      if (res && res->status == 200) {
        std::cout << "Successfully shared data with " << peer.client_id
                  << std::endl;
      } else {
        std::cout << "Failed to share data with " << peer.client_id
                  << std::endl;
      }

    } catch (const std::exception &e) {
      std::cout << "Exception sharing data with " << peer.client_id << ": "
                << e.what() << std::endl;
    }
  }
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
