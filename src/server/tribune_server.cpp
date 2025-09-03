#include "protocol/parser.hpp"
#include "server/tribune_server.hpp"
#include "crypto/signature.hpp"
#include <format>
#include <iostream>
#include <thread>
#include <vector>

TribuneServer::TribuneServer(const std::string &host, int port, const ServerConfig& config)
    : config_(config), host(host), port(port), rng_(rd_()),
      server_private_key_("server_private_key_placeholder"),
      server_public_key_("server_public_key_placeholder") {
  setupRoutes();
}

TribuneServer::~TribuneServer() {
  stop();
}
void TribuneServer::start() {
  std::cout << "Starting aggregator server on http://" << host << ":" << port
            << std::endl;
  
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
             std::cout << "CONNECT: Received data: " << req.body << std::endl;
             this->handleEndpointConnect(req, res);
           });

  svr.Post("/submit",
           [this](const httplib::Request &req, httplib::Response &res) {
             std::cout << "SUBMIT: Received data: " << req.body << std::endl;
             this->handleEndpointSubmit(req, res);
           });

  svr.Get("/peers",
          [this](const httplib::Request &req, httplib::Response &res) {
            std::cout << "PEERS: Received request: " << req.body << std::endl;
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
    std::cout << "Succesfully parsed ConnectResponse from Client with ID: "
              << parsed_res.client_id << std::endl;

    ClientState state(parsed_res.client_host, parsed_res.client_port,
                      parsed_res.client_id, parsed_res.ed25519_pub);

    std::cout << "Adding client to roster with ID: '" << parsed_res.client_id
              << "'" << std::endl;
    
    {
      std::lock_guard<std::mutex> lock(roster_mutex_);
      this->roster[parsed_res.client_id] = state;
      std::cout << "Roster size after adding: " << this->roster.size()
                << std::endl;
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
    std::cout << "=== COMPUTATION RESULT RECEIVED ===" << std::endl;
    std::cout << "From Client: " << parsed_res.client_id << std::endl;
    std::cout << "Event ID: " << parsed_res.event_id << std::endl;
    std::cout << "Result: " << parsed_res.data << std::endl;
    
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
      std::cout << "Progress: received " << received_count << "/" << expected_count << " sub results" << std::endl;
    }
    std::cout << "====================================" << std::endl;

    std::cout << "Checking if client '" << parsed_res.client_id
              << "' is in roster..." << std::endl;
    
    {
      std::lock_guard<std::mutex> roster_lock(roster_mutex_);
      std::cout << "Current roster contents:" << std::endl;
      for (const auto &[id, _] : this->roster) {
        std::cout << "  - '" << id << "'" << std::endl;
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
        std::cout
            << "Received valid SubmitResponse from Unconnected Client with ID: "
            << parsed_res.client_id << ", for Event: " << parsed_res.event_id
            << std::endl;
        res.status = 400;
        res.set_content("{\"error\":\"Client not connected\"}",
                        "application/json");
      }
    }

  } else {
    res.status = 400;
    std::cout << "Received invalid SubmitResponse" << std::endl;
    res.set_content("{\"error\":\"Invalid request\"}", "application/json");
  }
}

void TribuneServer::announceEvent(const Event& event) {
  // VALIDATE event before announcing to catch signature/timestamp issues
  if (event.server_signature.empty()) {
    std::cout << "ERROR: Event " << event.event_id << " has empty server signature!" << std::endl;
  }
  if (event.timestamp.time_since_epoch().count() == 0) {
    std::cout << "ERROR: Event " << event.event_id << " has zero timestamp!" << std::endl;
  }
  
  // Convert Event to JSON string using automatic conversion
  nlohmann::json j = event;
  std::string json_str = j.dump();

  std::cout << "Announcing event " << event.event_id << " to " 
            << event.participants.size() << " participants" << std::endl;
  std::cout << "Event signature: " << event.server_signature << std::endl;
  std::cout << "JSON being sent: " << json_str.substr(0, 200) << "..." << std::endl;
  std::cout << "Event timestamp: " << std::chrono::duration_cast<std::chrono::milliseconds>(
                event.timestamp.time_since_epoch()).count() << "ms" << std::endl;
  
  // Track this event as active
  {
    std::lock_guard<std::mutex> lock(active_events_mutex_);
    ActiveEvent active_event;
    active_event.event_id = event.event_id;
    active_event.computation_type = event.computation_type;
    active_event.expected_participants = static_cast<int>(event.participants.size());
    active_event.created_time = std::chrono::steady_clock::now();
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
          std::cout << "Sent Event with ID: " << event_id
                    << ", to Client: " << participant.client_host << ":"
                    << participant.client_port << " - Status: " << res->status << std::endl;
        } else {
          std::cout << "Failed to send Event to Client: " << participant.client_host 
                    << ":" << participant.client_port;
          if (res) {
            std::cout << " (Status: " << res->status << ")";
          }
          std::cout << std::endl;
        }
      } catch (const std::exception& e) {
        std::cout << "Exception sending event to " << participant.client_host 
                  << ":" << participant.client_port << ": " << e.what() << std::endl;
      }
    });
    
    // Small delay between thread creation to avoid overwhelming the system
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  
  // Wait for all announcements to complete
  for (auto& thread : announcement_threads) {
    thread.join();
  }
  
  std::cout << "All event announcements completed for event " << event.event_id << std::endl;
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
  
  std::cout << "Found " << active_clients.size() << " active clients" << std::endl;
  
  // Check minimum threshold
  if (static_cast<int>(active_clients.size()) < config_.min_participants) {
    std::cout << "Not enough participants (" << active_clients.size() 
              << " < " << config_.min_participants << ")" << std::endl;
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
  
  std::cout << "Selected " << selected.size() << " participants" << std::endl;
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
  std::cout << "SERVER: Creating signature for event hash: " << event_hash << std::endl;
  event.server_signature = SignatureUtils::createSignature(event_hash, server_private_key_);
  std::cout << "SERVER: Generated signature: " << event.server_signature << std::endl;
  
  return event;
}

void TribuneServer::registerComputation(const std::string& type, std::unique_ptr<MPCComputation> computation) {
  std::lock_guard<std::mutex> lock(computations_mutex_);
  computations_[type] = std::move(computation);
  std::cout << "Server registered MPC computation: " << type << std::endl;
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
      std::cout << "Event " << event_id << " is complete (" << received_count 
                << "/" << active_event.expected_participants << " responses)" << std::endl;
      
      // Process the complete results
      std::lock_guard<std::mutex> comp_lock(computations_mutex_);
      auto comp_it = computations_.find(active_event.computation_type);
      
      if (comp_it != computations_.end()) {
        std::string final_result = comp_it->second->aggregateResults(results);
        std::cout << "=== FINAL MPC RESULT ===" << std::endl;
        std::cout << "Event: " << event_id << std::endl;
        std::cout << "Computation: " << active_event.computation_type << std::endl;
        std::cout << "Final Result: " << final_result << std::endl;
        std::cout << "========================" << std::endl;
      } else {
        std::cout << "No computation handler for type: " << active_event.computation_type << std::endl;
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
  std::cout << "Started periodic event checker thread" << std::endl;
  
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
        
        if (age.count() > 30) { // 30 second timeout
          auto responses_it = unprocessed_responses.find(event_id);
          int received_count = responses_it != unprocessed_responses.end() 
                               ? static_cast<int>(responses_it->second.size()) 
                               : 0;
          
          std::cout << "Event " << event_id << " timed out after " << age.count() 
                    << " seconds with " << received_count << "/" << active_event.expected_participants 
                    << " responses" << std::endl;
          
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
        std::cout << "Active events: " << active_events_.size() << std::endl;
      }
    }
  }
  
  std::cout << "Periodic event checker thread stopped" << std::endl;
}

