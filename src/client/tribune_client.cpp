#include "client/tribune_client.hpp"
#include "protocol/parser.hpp"
#include "utils/logging.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

TribuneClient::TribuneClient(const std::string &seed_host, int seed_port,
                             int listen_port, const std::string &private_key,
                             const std::string &public_key)
    : seed_host_(seed_host), seed_port_(seed_port), listen_port_(listen_port),
      running_(false) {

  client_id_ = generateUUID();

  // Use provided keys or generate dummy ones
  if (!private_key.empty() && !public_key.empty()) {
    ed25519_private_key_ = private_key;
    ed25519_public_key_ = public_key;
    DEBUG_INFO("Using provided Ed25519 keypair");
  } else {
    ed25519_private_key_ = "dummy_private_key_" + client_id_;
    ed25519_public_key_ = "dummy_public_key_" + client_id_;
    DEBUG_INFO("Using generated dummy keypair");
  }

  setupEventRoutes();

  LOG("Created TribuneClient with ID: " << client_id_);
  DEBUG_INFO("Will connect to seed: " << seed_host_ << ":" << seed_port_);
  DEBUG_INFO("Listening on port: " << listen_port_);
}

TribuneClient::~TribuneClient() { stop(); }

void TribuneClient::setDataCollectionModule(
    std::unique_ptr<DataCollectionModule> module) {
  std::lock_guard<std::mutex> lock(data_module_mutex_);
  data_module_ = std::move(module);
  DEBUG_INFO("Data collection module updated");
}

void TribuneClient::registerComputation(
    const std::string &type, std::unique_ptr<MPCComputation> computation) {
  std::lock_guard<std::mutex> lock(computations_mutex_);
  computations_[type] = std::move(computation);
  DEBUG_INFO("Registered MPC computation: " << type);
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
    connect_req.ed25519_pub = ed25519_public_key_;

    // Convert to JSON
    nlohmann::json j = connect_req;
    std::string json_body = j.dump();

    LOG("Connecting to seed node...");
    auto res = cli.Post("/connect", json_body, "application/json");

    if (res && res->status == 200) {
      LOG("Successfully connected to seed node!");
      DEBUG_DEBUG("Response: " << res->body);

      // Parse response to extract server public key
      try {
        nlohmann::json response_json = nlohmann::json::parse(res->body);
        if (response_json.contains("server_public_key")) {
          server_public_key_ = response_json["server_public_key"];
          DEBUG_INFO("Received server public key: " << server_public_key_);
        } else {
          DEBUG_WARN("Server did not provide public key");
        }
      } catch (const std::exception &e) {
        DEBUG_WARN("Could not parse server response: " << e.what());
      }

      return true;
    } else {
      DEBUG_ERROR("Failed to connect to seed node. Status: "
                  << (res ? std::to_string(res->status) : "No response"));
      if (res) {
        DEBUG_DEBUG("Response body: " << res->body);
      }
      return false;
    }

  } catch (const std::exception &e) {
    DEBUG_ERROR("Exception during connection: " << e.what());
    return false;
  }
}

void TribuneClient::setupEventRoutes() {
  // Setup the /event endpoint to receive announcements
  event_server_.Post(
      "/event", [this](const httplib::Request &req, httplib::Response &res) {
        try {
          DEBUG_DEBUG("Received event announcement: " << req.body);

          // Parse the event
          nlohmann::json j = nlohmann::json::parse(req.body);
          Event event = j.get<Event>();

          DEBUG_DEBUG("Received event from server with signature: '"
                      << event.server_signature << "'");

          // Handle the event
          onEventAnnouncement(event);

          // Send response
          res.status = 200;
          res.set_content("{\"status\":\"received\"}", "application/json");

        } catch (const std::exception &e) {
          DEBUG_ERROR("Error processing event: " << e.what());
          res.status = 400;
          res.set_content("{\"error\":\"Failed to process event\"}",
                          "application/json");
        }
      });

  // Setup the /peer-data endpoint to receive data from other clients
  event_server_.Post("/peer-data", [this](const httplib::Request &req,
                                          httplib::Response &res) {
    try {

      // Parse the peer data message
      nlohmann::json j = nlohmann::json::parse(req.body);
      PeerDataMessage peer_msg = j.get<PeerDataMessage>();

      DEBUG_DEBUG("Received peer message with event_id: " << peer_msg.event_id);
      DEBUG_DEBUG(
          "Received original event ID: " << peer_msg.original_event.event_id);
      DEBUG_DEBUG(
          "JSON contains original_event: " << j.contains("original_event"));
      if (j.contains("original_event")) {
        DEBUG_DEBUG("original_event JSON has server_signature: "
                    << j["original_event"].contains("server_signature"));
        if (j["original_event"].contains("server_signature")) {
          DEBUG_DEBUG("server_signature value: '"
                      << j["original_event"]["server_signature"] << "'");
        }
      }

      // Handle the peer data with validation
      onPeerDataReceived(peer_msg);

      // Send response
      res.status = 200;
      res.set_content("{\"status\":\"received\"}", "application/json");

    } catch (const std::exception &e) {
      DEBUG_ERROR("Error processing peer data: " << e.what());
      res.status = 400;
      res.set_content("{\"error\":\"Failed to process peer data\"}",
                      "application/json");
    }
  });
}

void TribuneClient::startListening() {
  // Check if a data collection module is configured
  {
    std::lock_guard<std::mutex> lock(data_module_mutex_);
    if (!data_module_) {
      DEBUG_ERROR(
          "Cannot start listening: No data collection module configured!");
      DEBUG_ERROR("Call setDataCollectionModule() before startListening()");
      return;
    }
  }

  running_ = true;
  listener_thread_ = std::thread(&TribuneClient::runEventListener, this);
  LOG("Started event listener on port " << listen_port_);
}

void TribuneClient::runEventListener() {
  DEBUG_INFO("Event listener thread started");
  event_server_.listen("localhost", listen_port_);
}

void TribuneClient::onEventAnnouncement(const Event &event, bool relay) {
  LOG("=== EVENT RECEIVED ===");
  LOG("Event ID: " << event.event_id);
  DEBUG_INFO("Event Type: " << event.type_);
  DEBUG_INFO("Computation Type: " << event.computation_type);
  DEBUG_INFO("Participants: " << event.participants.size());
  LOG("=======================");

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
      DEBUG_DEBUG("Collected data: " << my_data);
    } else {
      // This should never happen if we prevent listening without a module
      DEBUG_ERROR(
          "INTERNAL ERROR: No data module configured but client is listening!");
      return;
    }
  }

  // Store our own data shard (data is now a plain string number)
  {
    std::lock_guard<std::mutex> shards_lock(event_shards_mutex_);
    try {
      std::string shard_value = my_data;
      event_shards_[event.event_id][client_id_] = shard_value;
      DEBUG_DEBUG("Stored our own shard for event "
                  << event.event_id << " (value: " << shard_value << ")");
    } catch (const std::exception &e) {
      DEBUG_ERROR("Error storing own data: " << e.what());
    }
  }

  // Add a small delay to ensure all participants receive the event announcement
  // before peer data sharing begins (prevents race conditions)
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  if (relay) {
    shareDataWithPeers(event, my_data);
  }
}

void TribuneClient::onPeerDataReceived(const PeerDataMessage &peer_msg) {
  LOG("=== PEER DATA RECEIVED ===");
  LOG("Event ID: " << peer_msg.event_id);
  LOG("From Client: " << peer_msg.from_client);
  DEBUG_DEBUG("===========================");

  // 1. Early age validation to prevent processing very old events
  auto now = std::chrono::steady_clock::now();
  auto now_sys = std::chrono::system_clock::now();

  long event_age = 0;
  if (!peer_msg.original_event.event_id.empty()) {
    auto event_time_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            peer_msg.original_event.timestamp.time_since_epoch())
            .count();
    auto now_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now_sys.time_since_epoch())
                           .count();

    DEBUG_DEBUG("Event timestamp: " << event_time_ms
                                    << "ms, Now: " << now_time_ms << "ms");

    if (event_time_ms == 0) {
      DEBUG_DEBUG("WARNING: Event " << peer_msg.original_event.event_id
                                    << " has zero timestamp!");
    }

    event_age = std::chrono::duration_cast<std::chrono::seconds>(
                    now_sys - peer_msg.original_event.timestamp)
                    .count();

    DEBUG_DEBUG("Event age: " << event_age << "s");
    if (event_age > EVENT_TIMEOUT_SECONDS) {
      DEBUG_DEBUG("Rejecting very old peer event (age: " << event_age << "s)");
      return;
    }
  }

  // 2. TTL-based deduplication to prevent broadcast storms
  std::string shard_key = peer_msg.event_id + "|" + peer_msg.from_client;

  {
    std::lock_guard<std::mutex> lock(recent_items_mutex_);

    // Only check for duplicate shards (allow shard processing even if we know
    // the event)
    if (recent_shards_.find(shard_key) != recent_shards_.end()) {
      DEBUG_DEBUG("Ignoring duplicate shard: " << shard_key);
      return;
    }

    // Mark shard as recently seen
    recent_shards_[shard_key] = {now};
  }

  // 3. Process peer-propagated event if we don't know about it
  bool have_event = false;
  {
    std::lock_guard<std::mutex> active_lock(active_events_mutex_);
    have_event = active_events_.find(peer_msg.event_id) != active_events_.end();
  }

  if (!have_event && !peer_msg.original_event.event_id.empty()) {
    DEBUG_DEBUG("Don't know about event "
                << peer_msg.event_id << ", processing peer-propagated event");

    // Age already validated in early check above

    // Verify server signature
    if (verifyEventFromServer(peer_msg.original_event)) {
      DEBUG_DEBUG("Valid server signature, processing peer event");
      onEventAnnouncement(peer_msg.original_event, false);
      have_event = true; // Update flag since we now have the event
    } else {
      DEBUG_DEBUG("Invalid server signature on peer event, rejecting");
      return;
    }
  }

  // 3. Now process the shard data (we should know about the event at this
  // point)
  if (!have_event) {
    DEBUG_DEBUG("Still don't know about event " << peer_msg.event_id
                                                << " after peer propagation");
    return;
  }

  std::lock_guard<std::mutex> active_lock(active_events_mutex_);
  auto event_it = active_events_.find(peer_msg.event_id);
  if (event_it == active_events_.end()) {
    DEBUG_DEBUG("Event " << peer_msg.event_id << " not found in active events");
    return;
  }

  // Check if sender is a valid participant
  bool valid_participant = false;
  std::string sender_public_key;

  DEBUG_DEBUG("Checking authorization for client " << peer_msg.from_client);
  DEBUG_DEBUG("Event " << peer_msg.event_id << " has "
                       << event_it->second.participants.size()
                       << " participants:");

  for (const auto &participant : event_it->second.participants) {
    DEBUG_DEBUG("- " << participant.client_id);
    if (participant.client_id == peer_msg.from_client) {
      valid_participant = true;
      sender_public_key = participant.ed25519_pub;
      break;
    }
  }

  if (!valid_participant) {
    DEBUG_DEBUG(
        "Rejected shard from unauthorized client: " << peer_msg.from_client);
    DEBUG_DEBUG("Client not found in participant list!");
    return;
  } else {
    DEBUG_DEBUG("Client authorized successfully");
  }

  // Verify shard signature
  std::string message =
      peer_msg.event_id + "|" + peer_msg.from_client + "|" + peer_msg.data;
  bool signature_valid = SignatureUtils::verifySignature(
      message, peer_msg.signature, sender_public_key);

  if (!signature_valid) {
    DEBUG_DEBUG(
        "Rejected shard with invalid signature from: " << peer_msg.from_client);
    return;
  }

  // Store the valid shard
  bool all_shards_received = false;
  {
    std::lock_guard<std::mutex> shards_lock(event_shards_mutex_);
    try {
      // peer_msg.data is now a plain string number, not JSON
      std::string shard_value = peer_msg.data;
      event_shards_[peer_msg.event_id][peer_msg.from_client] = shard_value;
      DEBUG_DEBUG("Stored valid shard from "
                  << peer_msg.from_client << " (value: " << shard_value << ")");
    } catch (const std::exception &e) {
      DEBUG_DEBUG("Error processing peer data: " << e.what());
      return;
    }

    all_shards_received = hasAllShards(peer_msg.event_id);
  }

  // Start computation if we have all shards
  if (all_shards_received) {
    DEBUG_DEBUG("All shards received for event " << peer_msg.event_id
                                                 << ", starting computation");
    std::thread([this, event_id = peer_msg.event_id]() {
      computeAndSubmitResult(event_id);
    }).detach();
  }

  // Periodic cleanup of deduplication caches
  // Triggered every CLEANUP_FREQUENCY peer messages to avoid timer threads
  if (++cleanup_counter_ % CLEANUP_FREQUENCY == 0) {
    cleanupRecentItems();
  }
}

void TribuneClient::shareDataWithPeers(const Event &event,
                                       const std::string &my_data) {
  DEBUG_INFO("Sharing data with peers for event: " << event.event_id);

  for (const auto &peer : event.participants) {
    // Skip ourselves
    if (peer.client_id == client_id_) {
      continue;
    }

    DEBUG_DEBUG("Sending data to peer: " << peer.client_id << " at "
                                         << peer.client_host << ":"
                                         << peer.client_port);

    // Split data into len(event.participants) - 1 shards

    try {
      httplib::Client cli(peer.client_host, std::stoi(peer.client_port));

      // Create data sharing payload
      PeerDataMessage peer_msg;
      peer_msg.event_id = event.event_id;
      peer_msg.from_client = client_id_;
      peer_msg.data = my_data;
      peer_msg.timestamp = std::chrono::system_clock::now();
      peer_msg.original_event =
          event; // Include server-signed event for propagation

      DEBUG_DEBUG("Creating peer message with event_id: " << peer_msg.event_id);
      DEBUG_DEBUG("Original event ID: " << peer_msg.original_event.event_id);

      // Create signature
      std::string message =
          peer_msg.event_id + "|" + peer_msg.from_client + "|" + peer_msg.data;
      peer_msg.signature =
          SignatureUtils::createSignature(message, ed25519_private_key_);

      nlohmann::json payload = peer_msg;

      auto res = cli.Post("/peer-data", payload.dump(), "application/json");

      if (res && res->status == 200) {
        DEBUG_DEBUG("Successfully shared data with " << peer.client_id);
      } else {
        DEBUG_WARN("Failed to share data with " << peer.client_id);
      }

    } catch (const std::exception &e) {
      DEBUG_ERROR("Exception sharing data with " << peer.client_id << ": "
                                                 << e.what());
    }
  }
}

bool TribuneClient::hasAllShards(const std::string &event_id) {
  // Must be called with active_events_mutex_ and event_shards_mutex_ held
  auto event_it = active_events_.find(event_id);
  if (event_it == active_events_.end()) {
    return false;
  }

  auto shards_it = event_shards_.find(event_id);
  if (shards_it == event_shards_.end()) {
    return false;
  }

  const Event &event = event_it->second;
  const auto &received_shards = shards_it->second;

  // Check if we have shards from all participants (including ourselves)
  for (const auto &participant : event.participants) {
    if (received_shards.find(participant.client_id) == received_shards.end()) {
      return false;
    }
  }

  return true;
}

void TribuneClient::computeAndSubmitResult(const std::string &event_id) {
  LOG("=== COMPUTING RESULT FOR EVENT: " << event_id << " ===");

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
      DEBUG_ERROR("Error: Event or shards not found for " << event_id);
      return;
    }

    event = event_it->second;
    computation_type = event.computation_type;

    // Collect shards in participant order for consistency
    for (const auto &participant : event.participants) {
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
      DEBUG_ERROR(
          "Error: No computation registered for type: " << computation_type);
      return;
    }

    result = comp_it->second->compute(shards, event.computation_metadata);
  }

  LOG("Computation complete! Result: " << result);

  // Send EventResponse back to server with result
  try {
    httplib::Client cli(seed_host_, seed_port_);

    EventResponse response;
    response.type_ = ResponseType::DataPart;
    response.event_id = event_id;
    response.client_id = client_id_;
    response.data = result;
    response.timestamp = std::chrono::system_clock::now();

    // Convert to JSON
    nlohmann::json j = response;
    std::string json_body = j.dump();

    DEBUG_INFO("Sending computation result to server...");
    auto res = cli.Post("/submit", json_body, "application/json");

    if (res && res->status == 200) {
      DEBUG_INFO("Successfully sent result to server!");
      DEBUG_DEBUG("Server response: " << res->body);
    } else {
      DEBUG_ERROR("Failed to send result to server. Status: "
                  << (res ? std::to_string(res->status) : "No response"));
      if (res) {
        DEBUG_DEBUG("Response body: " << res->body);
      }
    }

  } catch (const std::exception &e) {
    DEBUG_ERROR("Exception sending result to server: " << e.what());
  }
}

void TribuneClient::stop() {
  if (running_) {
    running_ = false;
    event_server_.stop();

    if (listener_thread_.joinable()) {
      listener_thread_.join();
    }

    LOG("Client stopped");
  }
}

void TribuneClient::cleanupRecentItems() {
  std::lock_guard<std::mutex> lock(recent_items_mutex_);
  auto now = std::chrono::steady_clock::now();

  // Clean expired events
  for (auto it = recent_events_.begin(); it != recent_events_.end();) {
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                   now - it->second.received_time)
                   .count();

    if (age > RECENT_ITEMS_TTL_SECONDS) {
      it = recent_events_.erase(it);
    } else {
      ++it;
    }
  }

  // Clean expired shards
  for (auto it = recent_shards_.begin(); it != recent_shards_.end();) {
    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                   now - it->second.received_time)
                   .count();

    if (age > RECENT_ITEMS_TTL_SECONDS) {
      it = recent_shards_.erase(it);
    } else {
      ++it;
    }
  }
}

bool TribuneClient::verifyEventFromServer(const Event &event) {
  // Create the same hash that the server would have created
  std::string event_hash = event.event_id + "|" + event.computation_type + "|" +
                           std::to_string(event.participants.size());

  DEBUG_DEBUG("CLIENT: Verifying event signature");
  DEBUG_DEBUG("CLIENT: Event hash: " << event_hash);
  DEBUG_DEBUG("CLIENT: Event server_signature: '" << event.server_signature
                                                  << "'");

  // Verify with server's public key
  return SignatureUtils::verifySignature(event_hash, event.server_signature,
                                         server_public_key_);
}
