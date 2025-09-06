#include "protocol/parser.hpp"
#include "server/tribune_server.hpp"
#include "crypto/signature.hpp"
#include "utils/logging.hpp"
#include <format>
#include <iostream>
#include <thread>
#include <vector>

TribuneServer::TribuneServer(const std::string &host, int port, const ServerConfig& config)
    : config_(config), host(host), port(port), rng_(rd_()) {
  // Generate real Ed25519 keypair for server
  auto keypair = SignatureUtils::generateKeyPair();
  server_public_key_ = keypair.first;
  server_private_key_ = keypair.second;
  
  LOG("Server initialized with Ed25519 public key: " << server_public_key_);
  setupRoutes();
}

TribuneServer::~TribuneServer() {
  stop();
}
void TribuneServer::start() {
  LOG("Starting aggregator server on http://" << host << ":" << port
           );
  
  // Start periodic event checker thread
  should_stop_ = false;
  checker_thread_ = std::thread(&TribuneServer::periodicEventChecker, this);
  
  svr.listen(host, port);
}

void TribuneServer::stop() {
  should_stop_ = true;
  if (checker_thread_.joinable()) {
    checker_thread_.join();
  }
  svr.stop();
}

void TribuneServer::setupRoutes() {
  // Simple GET endpoint
  svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
    res.set_content("Hello, World!", "text/plain");
  });
  svr.Post("/connect",
           [this](const httplib::Request &req, httplib::Response &res) {
             DEBUG_INFO("CONNECT: Received data: " << req.body);
             this->handleEndpointConnect(req, res);
           });

  svr.Post("/submit",
           [this](const httplib::Request &req, httplib::Response &res) {
             DEBUG_INFO("SUBMIT: Received data: " << req.body);
             this->handleEndpointSubmit(req, res);
           });

  svr.Get("/peers",
          [this](const httplib::Request &req, httplib::Response &res) {
            DEBUG_DEBUG("PEERS: Received request: " << req.body);
            this->handleEndpointPeers(req, res);
          });
}

void TribuneServer::handleEndpointPeers(const httplib::Request &req,
                                        httplib::Response &res) {

  std::string output = "{\"peers\":[";
  bool first = true;
  {
    std::lock_guard<std::mutex> lock(roster_mutex_);
    for (auto const &[key, val] : this->roster) {
      if (val.isClientParticipating()) {
        if (!first) {
          output += ",";
        }
        output += std::format("\"{}:{}\"", val.client_host, val.client_port);
        first = false;
      }
    }
  }
  output += "]}";
  res.status = 200;
  res.set_content(output, "application/json");
}

void TribuneServer::handleEndpointConnect(const httplib::Request &req,
                                          httplib::Response &res) {

  if (auto result = parseConnectResponse(req.body)) {
    ConnectResponse parsed_res = *result;
    DEBUG_DEBUG("Succesfully parsed ConnectResponse from Client with ID: "
              << parsed_res.client_id);

    ClientState state(parsed_res.client_host, parsed_res.client_port,
                      parsed_res.client_id, parsed_res.ed25519_pub);

    DEBUG_INFO("Adding client to roster with ID: '" << parsed_res.client_id
              << "'");
    
    {
      std::lock_guard<std::mutex> lock(roster_mutex_);
      this->roster[parsed_res.client_id] = state;
      DEBUG_DEBUG("Roster size after adding: " << this->roster.size()
               );
    }

    res.status = 200;
    nlohmann::json response = {
      {"received", true},
      {"server_public_key", server_public_key_}
    };
    res.set_content(response.dump(), "application/json");
  } else {
    res.status = 400;
    res.set_content("{\"error\":\"Invalid request\"}", "application/json");
  }
}
void TribuneServer::handleEndpointSubmit(const httplib::Request &req,
                                         httplib::Response &res) {

  if (auto result = parseSubmitResponse(req.body)) {
    EventResponse parsed_res = *result;
    DEBUG_DEBUG("=== COMPUTATION RESULT RECEIVED ===");
    DEBUG_DEBUG("From Client: " << parsed_res.client_id);
    DEBUG_DEBUG("Event ID: " << parsed_res.event_id);
    DEBUG_DEBUG("Result: " << parsed_res.data);
    
    // Show progress: received X/Y sub results
    int received_count = 0;
    int expected_count = 0;
    {
      std::lock_guard<std::mutex> responses_lock(unprocessed_responses_mutex_);
      std::lock_guard<std::mutex> events_lock(active_events_mutex_);
      
      auto responses_it = unprocessed_responses.find(parsed_res.event_id);
      if (responses_it != unprocessed_responses.end()) {
        received_count = static_cast<int>(responses_it->second.size()) + 1; // +1 for current result
      } else {
        received_count = 1;
      }
      
      auto active_it = active_events_.find(parsed_res.event_id);
      if (active_it != active_events_.end()) {
        expected_count = active_it->second.expected_participants;
      }
    }
    
    if (expected_count > 0) {
      DEBUG_DEBUG("Progress: received " << received_count << "/" << expected_count << " sub results");
    }
    DEBUG_DEBUG("====================================");

    DEBUG_DEBUG("Checking if client '" << parsed_res.client_id
              << "' is in roster...");
    
    {
      std::lock_guard<std::mutex> roster_lock(roster_mutex_);
      DEBUG_DEBUG("Current roster contents:");
      for (const auto &[id, _] : this->roster) {
        DEBUG_DEBUG("  - '" << id << "'");
      }

      if (this->roster.find(parsed_res.client_id) != this->roster.end()) {
        {
          std::lock_guard<std::mutex> responses_lock(unprocessed_responses_mutex_);
          this->unprocessed_responses[parsed_res.event_id][parsed_res.client_id] =
              parsed_res;
        }

        this->roster[parsed_res.client_id].markReceivedEvent();
        
        // Check if we can aggregate results for any completed events
        checkForCompleteResults();

        res.status = 200;
        res.set_content("{\"received\":true}", "application/json");
      } else {
        DEBUG_WARN("Received valid SubmitResponse from Unconnected Client with ID: "
            << parsed_res.client_id << ", for Event: " << parsed_res.event_id);
        res.status = 400;
        res.set_content("{\"error\":\"Client not connected\"}",
                        "application/json");
      }
    }

  } else {
    res.status = 400;
    DEBUG_DEBUG("Received invalid SubmitResponse");
    res.set_content("{\"error\":\"Invalid request\"}", "application/json");
  }
}

void TribuneServer::announceEvent(const Event& event, std::string* result) {
  // VALIDATE event before announcing to catch signature/timestamp issues
  if (event.server_signature.empty()) {
    DEBUG_ERROR("ERROR: Event " << event.event_id << " has empty server signature!");
  }
  if (event.timestamp.time_since_epoch().count() == 0) {
    DEBUG_ERROR("ERROR: Event " << event.event_id << " has zero timestamp!");
  }
  
  // Convert Event to JSON string using automatic conversion
  nlohmann::json j = event;
  std::string json_str = j.dump();

  DEBUG_DEBUG("Announcing event " << event.event_id << " to " 
            << event.participants.size() << " participants");
  DEBUG_DEBUG("Event signature: " << event.server_signature);
  DEBUG_DEBUG("JSON being sent: " << json_str.substr(0, 200) << "...");
  DEBUG_DEBUG("Event timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(
                event.timestamp.time_since_epoch()).count() << "ms");
  
  // Track this event as active
  {
    std::lock_guard<std::mutex> lock(active_events_mutex_);
    ActiveEvent active_event;
    active_event.event_id = event.event_id;
    active_event.computation_type = event.computation_type;
    active_event.expected_participants = static_cast<int>(event.participants.size());
    active_event.created_time = std::chrono::steady_clock::now();
    active_event.result_ptr = result;
    active_events_[event.event_id] = active_event;
  }

  // Send event announcements with timeouts and controlled concurrency
  std::vector<std::thread> announcement_threads;
  
  for (const auto& participant : event.participants) {
    announcement_threads.emplace_back([this, participant, json_str, event_id = event.event_id]() {
      try {
        httplib::Client cli(participant.client_host, std::stoi(participant.client_port));
        cli.set_connection_timeout(2, 0);  // 2 second connection timeout
        cli.set_read_timeout(5, 0);        // 5 second read timeout
        cli.set_write_timeout(5, 0);       // 5 second write timeout
        
        auto res = cli.Post("/event", json_str, "application/json");

        if (res && res->status == 200) {
          DEBUG_DEBUG("Sent Event with ID: " << event_id
                    << ", to Client: " << participant.client_host << ":"
                    << participant.client_port << " - Status: " << res->status);
        } else {
          DEBUG_DEBUG("Failed to send Event to Client: " << participant.client_host 
                    << ":" << participant.client_port 
                    << (res ? " (Status: " + std::to_string(res->status) + ")" : ""));
        }
      } catch (const std::exception& e) {
        DEBUG_ERROR("Exception sending event to " << participant.client_host 
                  << ":" << participant.client_port << ": " << e.what());
      }
    });
    
    // Small delay between thread creation to avoid overwhelming the system
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  // Wait for all announcements to complete
  for (auto& thread : announcement_threads) {
    thread.join();
  }
  
  DEBUG_DEBUG("All event announcements completed for event " << event.event_id);
}

std::vector<ClientInfo> TribuneServer::selectParticipants() {
  // Get all active participating clients
  std::vector<ClientInfo> active_clients;
  
  std::lock_guard<std::mutex> lock(roster_mutex_);
  for (const auto& [client_id, client_state] : roster) {
    if (client_state.isClientParticipating()) {
      ClientInfo info;
      info.client_id = client_state.client_id;
      info.client_host = client_state.client_host;
      info.client_port = client_state.client_port;
      info.ed25519_pub = client_state.ed25519_pub;
      active_clients.push_back(info);
    }
  }
  
  DEBUG_DEBUG("Found " << active_clients.size() << " active clients");
  
  // Check minimum threshold
  if (static_cast<int>(active_clients.size()) < config_.min_participants) {
    DEBUG_DEBUG("Not enough participants (" << active_clients.size() 
              << " < " << config_.min_participants << ")");
    return {};
  }
  
  // Determine participant count
  int participant_count = std::min(
    static_cast<int>(active_clients.size()),
    config_.max_participants
  );
  
  // Random selection
  {
    std::lock_guard<std::mutex> rng_lock(rng_mutex_);
    std::shuffle(active_clients.begin(), active_clients.end(), rng_);
  }
  
  std::vector<ClientInfo> selected(
    active_clients.begin(),
    active_clients.begin() + participant_count
  );
  
  DEBUG_DEBUG("Selected " << selected.size() << " participants");
  return selected;
}

std::optional<Event> TribuneServer::createEvent(EventType type, const std::string& event_id, const std::string& computation_type) {
  auto participants = selectParticipants();
  
  if (participants.empty()) {
    return std::nullopt;
  }
  
  Event event;
  event.type_ = type;
  event.event_id = event_id;
  event.computation_type = computation_type;
  event.participants = std::move(participants);
  event.timestamp = std::chrono::system_clock::now();
  
  // Create server signature for event verification  
  std::string event_hash = event.event_id + "|" + event.computation_type + "|" + std::to_string(event.participants.size());
  DEBUG_DEBUG("SERVER: Creating signature for event hash: " << event_hash);
  event.server_signature = SignatureUtils::createSignature(event_hash, server_private_key_);
  DEBUG_DEBUG("SERVER: Generated signature: " << event.server_signature);
  
  return event;
}

void TribuneServer::registerComputation(const std::string& type, std::unique_ptr<MPCComputation> computation) {
  std::lock_guard<std::mutex> lock(computations_mutex_);
  computations_[type] = std::move(computation);
  DEBUG_INFO("Server registered MPC computation: " << type);
}

void TribuneServer::checkForCompleteResults() {
  std::lock_guard<std::mutex> responses_lock(unprocessed_responses_mutex_);
  std::lock_guard<std::mutex> events_lock(active_events_mutex_);
  
  std::vector<std::string> completed_events;
  
  // Check each active event
  for (const auto& [event_id, active_event] : active_events_) {
    auto responses_it = unprocessed_responses.find(event_id);
    
    // Count received responses
    int received_count = 0;
    std::vector<std::string> results;
    
    if (responses_it != unprocessed_responses.end()) {
      received_count = static_cast<int>(responses_it->second.size());
      for (const auto& [client_id, response] : responses_it->second) {
        results.push_back(response.data);
      }
    }
    
    // Check if we have all expected responses
    if (received_count >= active_event.expected_participants) {
      DEBUG_DEBUG("Event " << event_id << " is complete (" << received_count 
                << "/" << active_event.expected_participants << " responses)");
      
      // Process the complete results
      std::lock_guard<std::mutex> comp_lock(computations_mutex_);
      auto comp_it = computations_.find(active_event.computation_type);
      
      if (comp_it != computations_.end()) {
        std::string final_result = comp_it->second->aggregateResults(results);
        DEBUG_DEBUG("=== FINAL MPC RESULT ===");
        DEBUG_DEBUG("Event: " << event_id);
        DEBUG_DEBUG("Computation: " << active_event.computation_type);
        DEBUG_DEBUG("Final Result: " << final_result);
        DEBUG_DEBUG("========================");
        
        // Store result in provided pointer if available
        if (active_event.result_ptr != nullptr) {
          *active_event.result_ptr = final_result;
        }
      } else {
        DEBUG_DEBUG("No computation handler for type: " << active_event.computation_type);
      }
      
      completed_events.push_back(event_id);
    }
  }
  
  // Clean up completed events
  for (const std::string& event_id : completed_events) {
    active_events_.erase(event_id);
    unprocessed_responses.erase(event_id);
  }
}

void TribuneServer::periodicEventChecker() {
  DEBUG_INFO("Started periodic event checker thread");
  
  while (!should_stop_) {
    // Sleep for 5 seconds between checks
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    if (should_stop_) break;
    
    // Check for timeout events (events older than 30 seconds)
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> timed_out_events;
    
    {
      std::lock_guard<std::mutex> events_lock(active_events_mutex_);
      std::lock_guard<std::mutex> responses_lock(unprocessed_responses_mutex_);
      
      for (const auto& [event_id, active_event] : active_events_) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - active_event.created_time);
        
        if (age.count() > 60) { // 60 second timeout for complex peer-to-peer operations
          auto responses_it = unprocessed_responses.find(event_id);
          int received_count = responses_it != unprocessed_responses.end() 
                               ? static_cast<int>(responses_it->second.size()) 
                               : 0;
          
          DEBUG_WARN("Event timed out after " << age.count() 
                    << " seconds with " << received_count << "/" << active_event.expected_participants 
                    << " responses");
          
          timed_out_events.push_back(event_id);
        }
      }
      
      // Clean up timed out events
      for (const std::string& event_id : timed_out_events) {
        active_events_.erase(event_id);
        unprocessed_responses.erase(event_id);
      }
    }
    
    // Check for complete results
    checkForCompleteResults();
    
    // Print status if there are active events
    {
      std::lock_guard<std::mutex> lock(active_events_mutex_);
      if (!active_events_.empty()) {
        DEBUG_DEBUG("Active events: " << active_events_.size());
      }
    }
  }
  
  DEBUG_INFO("Periodic event checker thread stopped");
}

