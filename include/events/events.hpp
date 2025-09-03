#pragma once
#include <chrono>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

enum EventType { DataSubmission = 0 };
enum ResponseType { DataPart = 0, ConnectionRequest };

struct ClientInfo {
  std::string client_id;
  std::string client_host;
  std::string client_port;
  std::string ed25519_pub;  // Public key for signature verification
};

struct Event {
  EventType type_;
  std::string event_id;
  std::string computation_type;
  std::vector<ClientInfo> participants;
};

struct EventResponse {
  ResponseType type_;
  std::string event_id;
  std::string client_id;
  std::string data;
  std::chrono::time_point<std::chrono::system_clock> timestamp;
};

struct ConnectResponse {
  ResponseType type_;
  std::string client_host;
  std::string client_port;
  std::string client_id;
  std::string ed25519_pub;
};

struct PeerDataMessage {
  std::string event_id;
  std::string from_client;
  std::string data;
  std::string signature;  // Ed25519 signature of (event_id + from_client + data)
  std::chrono::time_point<std::chrono::system_clock> timestamp;
};

// JSON conversion functions for ClientInfo
inline void to_json(nlohmann::json &j, const ClientInfo &c) {
  j = nlohmann::json{{"client_id", c.client_id}, {"client_host", c.client_host}, {"client_port", c.client_port}, {"ed25519_pub", c.ed25519_pub}};
}

inline void from_json(const nlohmann::json &j, ClientInfo &c) {
  j.at("client_id").get_to(c.client_id);
  j.at("client_host").get_to(c.client_host);
  j.at("client_port").get_to(c.client_port);
  j.at("ed25519_pub").get_to(c.ed25519_pub);
}

// JSON conversion functions for Event
inline void to_json(nlohmann::json &j, const Event &e) {
  j = nlohmann::json{{"type", e.type_}, {"event_id", e.event_id}, {"computation_type", e.computation_type}, {"participants", e.participants}};
}

inline void from_json(const nlohmann::json &j, Event &e) {
  j.at("type").get_to(e.type_);
  j.at("event_id").get_to(e.event_id);
  j.at("computation_type").get_to(e.computation_type);
  j.at("participants").get_to(e.participants);
}

// JSON conversion functions for EventResponse
inline void to_json(nlohmann::json &j, const EventResponse &r) {
  j = nlohmann::json{
      {"type", r.type_},
      {"event_id", r.event_id},
      {"client_id", r.client_id},
      {"data", r.data},
      {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        r.timestamp.time_since_epoch())
                        .count()}};
}

inline void from_json(const nlohmann::json &j, EventResponse &r) {
  j.at("type").get_to(r.type_);
  j.at("event_id").get_to(r.event_id);
  j.at("client_id").get_to(r.client_id);
  j.at("data").get_to(r.data);

  int64_t timestamp_ms = j.at("timestamp");
  r.timestamp = std::chrono::system_clock::time_point(
      std::chrono::milliseconds(timestamp_ms));
}

// JSON conversion functions for ConnectResponse
inline void to_json(nlohmann::json &j, const ConnectResponse &c) {
  j = nlohmann::json{{"type", c.type_},
                     {"client_host", c.client_host},
                     {"client_port", c.client_port},
                     {"client_id", c.client_id},
                     {"ed25519_pub", c.ed25519_pub}};
}

inline void from_json(const nlohmann::json &j, ConnectResponse &c) {
  j.at("type").get_to(c.type_);
  j.at("client_host").get_to(c.client_host);
  j.at("client_port").get_to(c.client_port);
  j.at("client_id").get_to(c.client_id);
  j.at("ed25519_pub").get_to(c.ed25519_pub);
}


// JSON conversion functions for PeerDataMessage
inline void to_json(nlohmann::json &j, const PeerDataMessage &p) {
  j = nlohmann::json{{"event_id", p.event_id},
                     {"from_client", p.from_client},
                     {"data", p.data},
                     {"signature", p.signature}};
}

inline void from_json(const nlohmann::json &j, PeerDataMessage &p) {
  j.at("event_id").get_to(p.event_id);
  j.at("from_client").get_to(p.from_client);
  j.at("data").get_to(p.data);
  j.at("signature").get_to(p.signature);
}
