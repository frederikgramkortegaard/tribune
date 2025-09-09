#include "crypto/signature.hpp"
#include "protocol/parser.hpp"
#include "server/tribune_server.hpp"
#include "utils/logging.hpp"
#include <format>
#include <iostream>
#include <shared_mutex>
#include <thread>
#include <vector>

TribuneServer::TribuneServer(const std::string &host, int port,
                             const ServerConfig &config)
    : config_(config), host_(host), port_(port), rng_(rd_()) {
  // Generate real Ed25519 keypair for server
  auto keypair = SignatureUtils::generateKeyPair();
  server_public_key_ = keypair.first;
  server_private_key_ = keypair.second;

  // Configure connection pool for TLS if enabled
  connection_pool_.setUseTLS(config_.use_tls);

  LOG("Server initialized with Ed25519 public key: " << server_public_key_);
}

TribuneServer::~TribuneServer() { stop(); }
void TribuneServer::start() {
  // Start periodic threads
  should_stop_ = false;
  checker_thread_ = std::thread(&TribuneServer::periodicEventChecker, this);
  ping_thread_ = std::thread(&TribuneServer::periodicPinger, this);

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  if (config_.use_tls) {
    LOG("Starting aggregator server on https://" << host_ << ":" << port_);
    ssl_svr_ = std::make_unique<httplib::SSLServer>(
        config_.cert_file.c_str(), 
        config_.private_key_file.c_str()
    );
    setupRoutesForServer(ssl_svr_.get());
    ssl_svr_->listen(host_, port_);
  } else {
#endif
    LOG("Starting aggregator server on http://" << host_ << ":" << port_);
    svr_ = std::make_unique<httplib::Server>();
    setupRoutesForServer(svr_.get());
    svr_->listen(host_, port_);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  }
#endif
}

void TribuneServer::stop() {
  should_stop_ = true;
  if (checker_thread_.joinable()) {
    checker_thread_.join();
  }
  if (ping_thread_.joinable()) {
    ping_thread_.join();
  }
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  if (ssl_svr_) {
    ssl_svr_->stop();
  }
#endif
  if (svr_) {
    svr_->stop();
  }
}

template<typename ServerType>
void TribuneServer::setupRoutesForServer(ServerType* server) {
  // Simple GET endpoint
  server->Get("/", [](const httplib::Request &, httplib::Response &res) {
    res.set_content("Hello, World!", "text/plain");
  });
  server->Post("/connect",
           [this](const httplib::Request &req, httplib::Response &res) {
             DEBUG_INFO("CONNECT: Received data: " << req.body);
             this->handleEndpointConnect(req, res);
           });

  server->Post("/submit",
           [this](const httplib::Request &req, httplib::Response &res) {
             DEBUG_INFO("SUBMIT: Received data: " << req.body);
             this->handleEndpointSubmit(req, res);
           });

  server->Get("/peers",
          [this](const httplib::Request &req, httplib::Response &res) {
            DEBUG_DEBUG("PEERS: Received request: " << req.body);
            this->handleEndpointPeers(req, res);
          });
  
  server->Post("/ping",
           [this](const httplib::Request &req, httplib::Response &res) {
             this->handleEndpointPing(req, res);
           });
}

// Explicit template instantiations
template void TribuneServer::setupRoutesForServer<httplib::Server>(httplib::Server*);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
template void TribuneServer::setupRoutesForServer<httplib::SSLServer>(httplib::SSLServer*);
#endif

void TribuneServer::handleEndpointPeers(const httplib::Request &req,
                                        httplib::Response &res) {

  std::string output = "{\"peers\":[";
  bool first = true;
  {
    std::shared_lock<std::shared_mutex> lock(roster_mutex_);
    for (auto const &[key, val] : this->roster_) {
      if (!first) {
        output += ",";
      }
      output += std::format("\"{}:{}\"", val.client_host_, val.client_port_);
      first = false;
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
    state.updatePingTime();

    DEBUG_INFO("Adding client to roster with ID: '" << parsed_res.client_id
                                                    << "'");

    {
      std::unique_lock<std::shared_mutex> lock(roster_mutex_);
      this->roster_[parsed_res.client_id] = state;
      DEBUG_DEBUG("Roster size after adding: " << this->roster_.size());
    }

    res.status = 200;
    nlohmann::json response = {{"received", true},
                               {"server_public_key", server_public_key_}};
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
      std::shared_lock<std::shared_mutex> responses_lock(unprocessed_responses_mutex_);
      std::shared_lock<std::shared_mutex> events_lock(active_events_mutex_);

      auto responses_it = unprocessed_responses_.find(parsed_res.event_id);
      if (responses_it != unprocessed_responses_.end()) {
        received_count = static_cast<int>(responses_it->second.size()) +
                         1; // +1 for current result
      } else {
        received_count = 1;
      }

      auto active_it = active_events_.find(parsed_res.event_id);
      if (active_it != active_events_.end()) {
        expected_count = active_it->second.expected_participants;
      }
    }

    if (expected_count > 0) {
      DEBUG_DEBUG("Progress: received " << received_count << "/"
                                        << expected_count << " sub results");
    }
    DEBUG_DEBUG("====================================");

    DEBUG_DEBUG("Checking if client '" << parsed_res.client_id
                                       << "' is in roster...");

    {
      std::shared_lock<std::shared_mutex> roster_lock(roster_mutex_);
      DEBUG_DEBUG("Current roster contents:");
      for (const auto &[id, _] : this->roster_) {
        DEBUG_DEBUG("  - '" << id << "'");
      }

      if (this->roster_.find(parsed_res.client_id) != this->roster_.end()) {
        roster_lock.unlock();
        {
          std::unique_lock<std::shared_mutex> responses_lock(
              unprocessed_responses_mutex_);
          this->unprocessed_responses_[parsed_res.event_id]
                                     [parsed_res.client_id] = parsed_res;
        }

        // Check if we can aggregate results for any completed events
        checkForCompleteResults();

        res.status = 200;
        res.set_content("{\"received\":true}", "application/json");
      } else {
        DEBUG_WARN(
            "Received valid SubmitResponse from Unconnected Client with ID: "
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

void TribuneServer::announceEvent(const Event &event, std::string *result) {
  // VALIDATE event before announcing to catch signature/timestamp issues
  if (event.server_signature.empty()) {
    DEBUG_ERROR("ERROR: Event " << event.event_id
                                << " has empty server signature!");
  }
  if (event.timestamp.time_since_epoch().count() == 0) {
    DEBUG_ERROR("ERROR: Event " << event.event_id << " has zero timestamp!");
  }

  // Convert Event to JSON string using automatic conversion
  nlohmann::json j = event;
  std::string json_str = j.dump();

  DEBUG_DEBUG("Announcing event " << event.event_id << " to "
                                  << event.participants.size()
                                  << " participants");
  DEBUG_DEBUG("Event signature: " << event.server_signature);
  DEBUG_DEBUG("JSON being sent: " << json_str.substr(0, 200) << "...");
  DEBUG_DEBUG("Event timestamp: "
              << std::chrono::duration_cast<std::chrono::milliseconds>(
                     event.timestamp.time_since_epoch())
                     .count()
              << "ms");

  {
    std::unique_lock<std::shared_mutex> lock(active_events_mutex_);
    active_events_.emplace(event.event_id,
                           ActiveEvent{
                               .event_id = event.event_id,
                               .computation_type = event.computation_type,
                               .expected_participants =
                                   static_cast<int>(event.participants.size()),
                               .created_time = std::chrono::steady_clock::now(),
                               .result_ptr = result,
                               .event = event // Store the actual event
                           });
  }

  // Send event announcements with timeouts and controlled concurrency
  std::vector<std::thread> announcement_threads;

  for (const auto &participant : event.participants) {
    announcement_threads.emplace_back([this, participant, json_str,
                                       event_id = event.event_id]() {
      try {
        connection_pool_.withConnection(participant.client_host,
                                       std::stoi(participant.client_port),
                                       [&](auto* client) {
          auto res = client->Post("/event", json_str, "application/json");

          if (res && res->status == 200) {
            DEBUG_DEBUG("Sent Event with ID: " << event_id << ", to Client: "
                                               << participant.client_host << ":"
                                               << participant.client_port
                                               << " - Status: " << res->status);
          } else {
            DEBUG_DEBUG(
                "Failed to send Event to Client: "
                << participant.client_host << ":" << participant.client_port
                << (res ? " (Status: " + std::to_string(res->status) + ")" : ""));
          }
          return true;
        });
      } catch (const std::exception &e) {
        DEBUG_ERROR("Exception sending event to "
                    << participant.client_host << ":" << participant.client_port
                    << ": " << e.what());
      }
    });

    // Small delay between thread creation to avoid overwhelming the system
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  // Wait for all announcements to complete
  for (auto &thread : announcement_threads) {
    thread.join();
  }

  DEBUG_DEBUG("All event announcements completed for event " << event.event_id);
}

std::vector<ClientInfo> TribuneServer::selectParticipants() {
  // Get all active participating clients
  std::vector<ClientInfo> active_clients;

  std::shared_lock<std::shared_mutex> lock(roster_mutex_);
  for (const auto &[client_id, client_state] : roster_) {
    ClientInfo info;
    info.client_id = client_state.client_id_;
    info.client_host = client_state.client_host_;
    info.client_port = client_state.client_port_;
    info.ed25519_pub = client_state.ed25519_pub_;
    active_clients.push_back(info);
  }

  DEBUG_DEBUG("Found " << active_clients.size() << " active clients");

  // Check minimum threshold
  if (static_cast<int>(active_clients.size()) < config_.min_participants) {
    DEBUG_DEBUG("Not enough participants (" << active_clients.size() << " < "
                                            << config_.min_participants << ")");
    return {};
  }

  // Determine participant count
  int participant_count = std::min(static_cast<int>(active_clients.size()),
                                   config_.max_participants);

  // Random selection
  {
    std::lock_guard<std::mutex> rng_lock(rng_mutex_);
    std::shuffle(active_clients.begin(), active_clients.end(), rng_);
  }

  std::vector<ClientInfo> selected(active_clients.begin(),
                                   active_clients.begin() + participant_count);

  DEBUG_DEBUG("Selected " << selected.size() << " participants");
  return selected;
}

std::optional<Event>
TribuneServer::createEvent(EventType type, const std::string &event_id,
                           const std::string &computation_type) {
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
  std::string event_hash = event.event_id + "|" + event.computation_type + "|" +
                           std::to_string(event.participants.size());
  DEBUG_DEBUG("SERVER: Creating signature for event hash: " << event_hash);
  event.server_signature =
      SignatureUtils::createSignature(event_hash, server_private_key_);
  DEBUG_DEBUG("SERVER: Generated signature: " << event.server_signature);

  return event;
}

void TribuneServer::registerModule(
    const std::string &type, std::unique_ptr<MPCModule> module) {
  std::unique_lock<std::shared_mutex> lock(modules_mutex_);
  modules_[type] = std::move(module);
  DEBUG_INFO("Server registered MPC computation: " << type);
}

void TribuneServer::handleEndpointPing(const httplib::Request &req,
                                       httplib::Response &res) {
  if (auto result = parseSubmitResponse(req.body)) {
    EventResponse parsed_res = *result;
    
    std::unique_lock<std::shared_mutex> lock(roster_mutex_);
    if (roster_.find(parsed_res.client_id) != roster_.end()) {
      roster_[parsed_res.client_id].updatePingTime();
      res.status = 200;
      res.set_content("{\"status\":\"pong\"}", "application/json");
    } else {
      res.status = 404;
      res.set_content("{\"error\":\"Client not found\"}", "application/json");
    }
  } else {
    res.status = 400;
    res.set_content("{\"error\":\"Invalid ping\"}", "application/json");
  }
}

void TribuneServer::checkForCompleteResults() {
  std::shared_lock<std::shared_mutex> responses_lock(unprocessed_responses_mutex_);
  std::shared_lock<std::shared_mutex> events_lock(active_events_mutex_);

  std::vector<std::string> completed_events;

  // Check each active event
  for (const auto &[event_id, active_event] : active_events_) {
    auto responses_it = unprocessed_responses_.find(event_id);

    // Count received responses
    int received_count = 0;
    std::vector<std::string> results;

    if (responses_it != unprocessed_responses_.end()) {
      received_count = static_cast<int>(responses_it->second.size());
      for (const auto &[client_id, response] : responses_it->second) {
        results.push_back(response.data);
      }
    }

    // Check if we have all expected responses
    if (received_count >= active_event.expected_participants) {
      DEBUG_DEBUG("Event " << event_id << " is complete (" << received_count
                           << "/" << active_event.expected_participants
                           << " responses)");

      std::shared_lock<std::shared_mutex> mod_lock(modules_mutex_);
      auto mod_it = modules_.find(active_event.computation_type);

      if (mod_it != modules_.end()) {
        // Convert string results to PartialResult objects
        std::vector<PartialResult> partials;
        for (size_t i = 0; i < results.size(); i++) {
          PartialResult partial;
          partial.participant_id = "participant_" + std::to_string(i);
          partial.value = nlohmann::json::parse(results[i]);
          partials.push_back(partial);
        }
        
        // Aggregate the partial results
        FinalResult final = mod_it->second->aggregate(partials, &active_event.event);
        std::string final_result = final.value.dump();
        
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
        DEBUG_DEBUG("No module handler for type: "
                    << active_event.computation_type);
      }

      completed_events.push_back(event_id);
    }
  }

  if (!completed_events.empty()) {
    events_lock.unlock();
    responses_lock.unlock();
    
    std::unique_lock<std::shared_mutex> write_events_lock(active_events_mutex_);
    std::unique_lock<std::shared_mutex> write_responses_lock(unprocessed_responses_mutex_);
    
    for (const std::string &event_id : completed_events) {
      active_events_.erase(event_id);
      unprocessed_responses_.erase(event_id);
    }
  }
}

void TribuneServer::periodicEventChecker() {
  DEBUG_INFO("Started periodic event checker thread");

  while (!should_stop_) {
    // Sleep for 5 seconds between checks
    std::this_thread::sleep_for(std::chrono::seconds(5));

    if (should_stop_)
      break;

    // Check for timeout events (events older than X seconds)
    auto now = std::chrono::steady_clock::now();
    std::vector<std::string> timed_out_events;

    {
      std::shared_lock<std::shared_mutex> events_lock(active_events_mutex_);
      std::shared_lock<std::shared_mutex> responses_lock(unprocessed_responses_mutex_);

      for (const auto &[event_id, active_event] : active_events_) {
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - active_event.created_time);

        if (age.count() >
            config_.event_timeout_boundary) { // X second timeout for complex
                                              // peer-to-peer operations
          auto responses_it = unprocessed_responses_.find(event_id);
          int received_count =
              responses_it != unprocessed_responses_.end()
                  ? static_cast<int>(responses_it->second.size())
                  : 0;

          DEBUG_WARN("Event timed out after "
                     << age.count() << " seconds with " << received_count << "/"
                     << active_event.expected_participants << " responses");

          timed_out_events.push_back(event_id);
        }
      }

      if (!timed_out_events.empty()) {
        events_lock.unlock();
        responses_lock.unlock();
        
        std::unique_lock<std::shared_mutex> write_events_lock(active_events_mutex_);
        std::unique_lock<std::shared_mutex> write_responses_lock(unprocessed_responses_mutex_);
        
        for (const std::string &event_id : timed_out_events) {
          active_events_.erase(event_id);
          unprocessed_responses_.erase(event_id);
        }
      }
    }

    // Check for complete results
    checkForCompleteResults();

    // Print status if there are active events
    {
      std::shared_lock<std::shared_mutex> lock(active_events_mutex_);
      if (!active_events_.empty()) {
        DEBUG_DEBUG("Active events: " << active_events_.size());
      }
    }
  }

  DEBUG_INFO("Periodic event checker thread stopped");
}

void TribuneServer::periodicPinger() {
  DEBUG_INFO("Started periodic ping thread");
  
  while (!should_stop_) {
    std::this_thread::sleep_for(std::chrono::seconds(config_.ping_interval_seconds));
    
    if (should_stop_) break;
    
    // Clean up expired connections
    connection_pool_.cleanupExpiredConnections();
    
    std::vector<std::string> dead_clients;
    {
      std::shared_lock<std::shared_mutex> roster_lock(roster_mutex_);
      for (const auto &[client_id, client_state] : roster_) {
        if (!client_state.isAlive(config_.client_timeout_seconds)) {
          dead_clients.push_back(client_id);
        }
      }
    }
    
    if (!dead_clients.empty()) {
      std::vector<std::string> removable_clients;
      {
        std::shared_lock<std::shared_mutex> events_lock(active_events_mutex_);
        for (const std::string &client_id : dead_clients) {
          bool in_active_event = false;
          for (const auto &[event_id, active_event] : active_events_) {
            for (const auto &participant : active_event.event.participants) {
              if (participant.client_id == client_id) {
                in_active_event = true;
                break;
              }
            }
            if (in_active_event) break;
          }
          
          if (!in_active_event) {
            removable_clients.push_back(client_id);
          }
        }
      }
      
      if (!removable_clients.empty()) {
        std::unique_lock<std::shared_mutex> lock(roster_mutex_);
        for (const std::string &client_id : removable_clients) {
          DEBUG_INFO("Removing dead client: " << client_id);
          
          // Remove pooled connection for this client
          connection_pool_.removeConnection(roster_[client_id].client_host_,
                                            std::stoi(roster_[client_id].client_port_));
          
          roster_.erase(client_id);
        }
      }
    }
  }
  
  DEBUG_INFO("Periodic ping thread stopped");
}
