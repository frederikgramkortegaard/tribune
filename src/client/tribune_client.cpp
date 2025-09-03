#include "client/data_collection_module.hpp"
#include "client/tribune_client.hpp"
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
  ed25519_private_key_ = "dummy_private_key_" + client_id_; // TODO: Generate real Ed25519 keypair
  setupEventRoutes();

  // Set default mock data collection module
  data_module_ = std::make_unique<MockDataCollectionModule>(client_id_);

  std::cout << "Created TribuneClient with ID: " << client_id_ << std::endl;
  std::cout << "Will connect to seed: " << seed_host_ << ":" << seed_port_
            << std::endl;
  std::cout << "Listening on port: " << listen_port_ << std::endl;
}

TribuneClient::~TribuneClient() { stop(); }

void TribuneClient::setDataCollectionModule(
    std::unique_ptr<DataCollectionModule> module) {
  std::lock_guard<std::mutex> lock(data_module_mutex_);
  data_module_ = std::move(module);
  std::cout << "Data collection module updated" << std::endl;
}

void TribuneClient::registerComputation(const std::string& type, std::unique_ptr<MPCComputation> computation) {
  std::lock_guard<std::mutex> lock(computations_mutex_);
  computations_[type] = std::move(computation);
  std::cout << "Registered MPC computation: " << type << std::endl;
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
    connect_req.ed25519_pub = "dummy_ed25519_key"; // TODO: Generate real Ed25519 public key

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

      // Handle the peer data with validation
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
  std::cout << "Computation Type: " << event.computation_type << std::endl;
  std::cout << "Participants: " << event.participants.size() << std::endl;
  std::cout << "======================" << std::endl;

  // Store event for validation and computation
  {
    std::lock_guard<std::mutex> lock(active_events_mutex_);
    active_events_[event.event_id] = event;
  }

  // Use data collection module to get client's data for this event
  std::string my_data;
  {
    std::lock_guard<std::mutex> lock(data_module_mutex_);
    if (data_module_) {
      my_data = data_module_->collectData(event);
      std::cout << "Collected data: " << my_data << std::endl;
    } else {
      my_data = "no_data_module_configured";
      std::cout << "Warning: No data collection module configured!"
                << std::endl;
    }
  }

  // Store our own data shard
  {
    std::lock_guard<std::mutex> shards_lock(event_shards_mutex_);
    event_shards_[event.event_id][client_id_] = my_data;
    std::cout << "Stored our own shard for event " << event.event_id << std::endl;
  }

  shareDataWithPeers(event, my_data);
}

void TribuneClient::onPeerDataReceived(const PeerDataMessage &peer_msg) {
  std::cout << "=== PEER DATA RECEIVED ===" << std::endl;
  std::cout << "Event ID: " << peer_msg.event_id << std::endl;
  std::cout << "From Client: " << peer_msg.from_client << std::endl;
  std::cout << "Data: " << peer_msg.data << std::endl;
  std::cout << "===========================" << std::endl;

  // Save the received shard
  {
    std::lock_guard<std::mutex> active_lock(active_events_mutex_);
    auto it = active_events_.find(peer_msg.event_id);

    // We're already aware of this event, validate and add this shard
    if (it != active_events_.end()) {
      // Check if this client is actually an expected participant of the event
      bool valid_participant = false;
      std::string sender_public_key;
      
      for (const auto &participant : it->second.participants) {
        if (participant.client_id == peer_msg.from_client) {
          valid_participant = true;
          sender_public_key = participant.ed25519_pub;
          break;
        }
      }

      if (valid_participant) {
        // Verify signature
        std::string message = peer_msg.event_id + "|" + peer_msg.from_client + "|" + peer_msg.data;
        bool signature_valid = SignatureUtils::verifySignature(message, peer_msg.signature, sender_public_key);
        
        if (signature_valid) {
          bool all_shards_received = false;
          {
            std::lock_guard<std::mutex> shards_lock(event_shards_mutex_);
            event_shards_[peer_msg.event_id][peer_msg.from_client] = peer_msg.data;
            std::cout << "Stored valid and authenticated shard from " << peer_msg.from_client
                      << std::endl;
            
            // Check if we now have all shards
            all_shards_received = hasAllShards(peer_msg.event_id);
          }
          
          // If complete, spawn thread to compute result
          if (all_shards_received) {
            std::cout << "All shards received for event " << peer_msg.event_id << ", starting computation" << std::endl;
            std::thread([this, event_id = peer_msg.event_id]() {
              computeAndSubmitResult(event_id);
            }).detach();
          }
        } else {
          std::cout << "Rejected shard with invalid signature from: "
                    << peer_msg.from_client << std::endl;
        }
      } else {
        std::cout << "Rejected shard from unauthorized client: "
                  << peer_msg.from_client << std::endl;
      }

      // We don't know this event yet, put it in orphan shards for later
      // validation
    } else {
      std::lock_guard<std::mutex> orphan_lock(orphan_shards_mutex_);
      
      // If we're at capacity, remove the oldest orphan event
      while (orphan_shards_.size() >= MAX_ORPHAN_EVENTS && !orphan_order_.empty()) {
        std::string oldest_event = orphan_order_.front();
        orphan_order_.pop();
        orphan_shards_.erase(oldest_event);
        std::cout << "Evicted oldest orphan event: " << oldest_event << std::endl;
      }
      
      // Add new orphan (or add to existing orphan event)
      if (orphan_shards_.find(peer_msg.event_id) == orphan_shards_.end()) {
        orphan_order_.push(peer_msg.event_id);
      }
      orphan_shards_[peer_msg.event_id][peer_msg.from_client] = peer_msg.data;
      std::cout << "Stored orphan shard from " << peer_msg.from_client
                << " for unknown event" << std::endl;
    }
  }

  // TODO: Check if we have data from all expected peers
  // TODO: Perform computation when ready
  // TODO: Send EventResponse back to server
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
      PeerDataMessage peer_msg;
      peer_msg.event_id = event.event_id;
      peer_msg.from_client = client_id_;
      peer_msg.data = my_data;
      peer_msg.timestamp = std::chrono::system_clock::now();
      
      // Create signature
      std::string message = peer_msg.event_id + "|" + peer_msg.from_client + "|" + peer_msg.data;
      peer_msg.signature = SignatureUtils::createSignature(message, ed25519_private_key_);

      nlohmann::json payload = peer_msg;

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

bool TribuneClient::hasAllShards(const std::string& event_id) {
  // Must be called with active_events_mutex_ and event_shards_mutex_ held
  auto event_it = active_events_.find(event_id);
  if (event_it == active_events_.end()) {
    return false;
  }

  auto shards_it = event_shards_.find(event_id);
  if (shards_it == event_shards_.end()) {
    return false;
  }

  const Event& event = event_it->second;
  const auto& received_shards = shards_it->second;
  
  // Check if we have shards from all participants (including ourselves)
  for (const auto& participant : event.participants) {
    if (received_shards.find(participant.client_id) == received_shards.end()) {
      return false;
    }
  }
  
  return true;
}

void TribuneClient::computeAndSubmitResult(const std::string& event_id) {
  std::cout << "=== COMPUTING RESULT FOR EVENT: " << event_id << " ===" << std::endl;
  
  Event event;
  std::vector<std::string> shards;
  std::string computation_type;
  
  // Collect event info and shards
  {
    std::lock_guard<std::mutex> active_lock(active_events_mutex_);
    std::lock_guard<std::mutex> shards_lock(event_shards_mutex_);
    
    auto event_it = active_events_.find(event_id);
    auto shards_it = event_shards_.find(event_id);
    
    if (event_it == active_events_.end() || shards_it == event_shards_.end()) {
      std::cout << "Error: Event or shards not found for " << event_id << std::endl;
      return;
    }
    
    event = event_it->second;
    computation_type = event.computation_type;
    
    // Collect shards in participant order for consistency
    for (const auto& participant : event.participants) {
      auto shard_it = shards_it->second.find(participant.client_id);
      if (shard_it != shards_it->second.end()) {
        shards.push_back(shard_it->second);
      }
    }
  }
  
  // Find and execute computation
  std::string result;
  {
    std::lock_guard<std::mutex> comp_lock(computations_mutex_);
    auto comp_it = computations_.find(computation_type);
    
    if (comp_it == computations_.end()) {
      std::cout << "Error: No computation registered for type: " << computation_type << std::endl;
      return;
    }
    
    result = comp_it->second->compute(shards);
  }
  
  std::cout << "Computation complete! Result: " << result << std::endl;
  
  // TODO: Send EventResponse back to server with result
  std::cout << "TODO: Send result '" << result << "' back to server for event " << event_id << std::endl;
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
