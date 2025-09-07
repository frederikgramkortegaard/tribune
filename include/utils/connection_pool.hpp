#pragma once
#include <httplib.h>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <chrono>
#include <variant>

class ConnectionPool {
private:
    struct PooledConnection {
        std::variant<std::unique_ptr<httplib::Client>, std::unique_ptr<httplib::SSLClient>> client;
        std::chrono::steady_clock::time_point last_used;
        std::string host;
        int port;
        bool use_tls;
        
        PooledConnection(const std::string& h, int p, bool tls = false) 
            : host(h), port(p), use_tls(tls) {
            if (use_tls) {
                auto ssl_client = std::make_unique<httplib::SSLClient>(host, port);
                ssl_client->enable_server_certificate_verification(false);
                ssl_client->set_connection_timeout(2, 0);
                ssl_client->set_read_timeout(5, 0);
                ssl_client->set_write_timeout(5, 0);
                client = std::move(ssl_client);
            } else {
                auto http_client = std::make_unique<httplib::Client>(host, port);
                http_client->set_connection_timeout(2, 0);
                http_client->set_read_timeout(5, 0);
                http_client->set_write_timeout(5, 0);
                client = std::move(http_client);
            }
            last_used = std::chrono::steady_clock::now();
        }
        
        bool isExpired(int timeout_seconds) const {
            auto now = std::chrono::steady_clock::now();
            auto age = std::chrono::duration_cast<std::chrono::seconds>(now - last_used);
            return age.count() >= timeout_seconds;
        }
        
        void updateLastUsed() {
            last_used = std::chrono::steady_clock::now();
        }
    };
    
    std::unordered_map<std::string, std::shared_ptr<PooledConnection>> connections_;
    std::shared_mutex connections_mutex_;
    
    static constexpr int CONNECTION_TIMEOUT_SECONDS = 60;
    bool use_tls_ = false;
    
    std::string makeKey(const std::string& host, int port) const {
        return host + ":" + std::to_string(port);
    }
    
public:
    void setUseTLS(bool use_tls) { use_tls_ = use_tls; }
    
    template<typename Func>
    auto withConnection(const std::string& host, int port, Func&& func) -> decltype(func(std::declval<httplib::Client*>())) {
        std::string key = makeKey(host, port);
        
        std::shared_ptr<PooledConnection> conn;
        {
            std::shared_lock<std::shared_mutex> lock(connections_mutex_);
            auto it = connections_.find(key);
            if (it != connections_.end() && !it->second->isExpired(CONNECTION_TIMEOUT_SECONDS)) {
                it->second->updateLastUsed();
                conn = it->second;
            }
        }
        
        if (!conn) {
            std::unique_lock<std::shared_mutex> lock(connections_mutex_);
            auto it = connections_.find(key);
            if (it == connections_.end() || it->second->isExpired(CONNECTION_TIMEOUT_SECONDS)) {
                connections_[key] = std::make_shared<PooledConnection>(host, port, use_tls_);
            }
            conn = connections_[key];
        }
        
        // Call the function with the appropriate client type
        return std::visit([&func](auto& client) {
            return func(client.get());
        }, conn->client);
    }
    
    void removeConnection(const std::string& host, int port) {
        std::string key = makeKey(host, port);
        std::unique_lock<std::shared_mutex> lock(connections_mutex_);
        connections_.erase(key);
    }
    
    void cleanupExpiredConnections() {
        std::vector<std::string> expired_keys;
        
        {
            std::shared_lock<std::shared_mutex> lock(connections_mutex_);
            for (const auto& [key, conn] : connections_) {
                if (conn->isExpired(CONNECTION_TIMEOUT_SECONDS)) {
                    expired_keys.push_back(key);
                }
            }
        }
        
        if (!expired_keys.empty()) {
            std::unique_lock<std::shared_mutex> lock(connections_mutex_);
            for (const std::string& key : expired_keys) {
                connections_.erase(key);
            }
        }
    }
    
    std::vector<std::string> getActiveConnections() {
        std::shared_lock<std::shared_mutex> lock(connections_mutex_);
        std::vector<std::string> keys;
        for (const auto& [key, conn] : connections_) {
            if (!conn->isExpired(CONNECTION_TIMEOUT_SECONDS)) {
                keys.push_back(key);
            }
        }
        return keys;
    }
};