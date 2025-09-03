#include "protocol/parser.hpp"
#include "server/tribune_server.hpp"
#include <format>
#include <iostream>

TribuneServer::TribuneServer(const std::string &host, int port)
    : host(host), port(port) {
  setupRoutes();
}
void TribuneServer::start() {
  std::cout << "Starting aggregator server on http://" << host << ":" << port
            << std::endl;
  svr.listen(host, port);
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
  for (auto const &[key, val] : this->roster) {
    if (val.isClientParticipating()) {
      if (!first) {
        output += ",";
      }
      output += std::format("\"{}:{}\"", val.client_host, val.client_port);
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
    std::cout << "Succesfully parsed ConnectResponse from Client with ID: "
              << parsed_res.client_id << std::endl;

    ClientState state(parsed_res.client_host, parsed_res.client_port,
                      parsed_res.client_id, parsed_res.x25519_pub,
                      parsed_res.ed25519_pub);

    std::cout << "Adding client to roster with ID: '" << parsed_res.client_id
              << "'" << std::endl;
    this->roster[parsed_res.client_id] = state;
    std::cout << "Roster size after adding: " << this->roster.size()
              << std::endl;

    res.status = 200;
    res.set_content("{\"received\":true}", "application/json");
  } else {
    res.status = 400;
    res.set_content("{\"error\":\"Invalid request\"}", "application/json");
  }
}
void TribuneServer::handleEndpointSubmit(const httplib::Request &req,
                                         httplib::Response &res) {

  if (auto result = parseSubmitResponse(req.body)) {
    EventResponse parsed_res = *result;
    std::cout << "Succesfully parsed SubmitResponse from Client with ID: "
              << parsed_res.client_id << ", for Event: " << parsed_res.event_id
              << std::endl;

    std::cout << "Checking if client '" << parsed_res.client_id
              << "' is in roster..." << std::endl;
    std::cout << "Current roster contents:" << std::endl;
    for (const auto &[id, _] : this->roster) {
      std::cout << "  - '" << id << "'" << std::endl;
    }

    if (this->roster.find(parsed_res.client_id) != this->roster.end()) {
      this->unprocessed_responses[parsed_res.event_id][parsed_res.client_id] =
          parsed_res;

      this->roster[parsed_res.client_id].markReceivedEvent();

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

  } else {
    res.status = 400;
    std::cout << "Received invalid SubmitResponse" << std::endl;
    res.set_content("{\"error\":\"Invalid request\"}", "application/json");
  }
}

void TribuneServer::announceEvent(const Event event) {
  // Convert Event to JSON string using automatic conversion
  nlohmann::json j = event;
  std::string json_str = j.dump();

  std::cout << "Announcing Event: " << event.event_id << std::endl;
  for (const auto &[id, state] : this->roster) {
    if (state.isClientParticipating()) {
      httplib::Client cli(state.client_host, std::stoi(state.client_port));
      auto res = cli.Post("/event", json_str, "application/json");

      if (res) {
        std::cout << "Sent Event with ID: " << event.event_id
                  << ", to Client: " << state.client_host << ":"
                  << state.client_port << " who accepted it" << std::endl;
      }
    }
  }
}

