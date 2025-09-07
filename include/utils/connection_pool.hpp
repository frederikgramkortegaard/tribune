#pragma once
#include <httplib.h>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <chrono>

class ConnectionPool {
private:
    struct PooledConnection {
        std::unique_ptr<httplib::Client> client;
        std::chrono::steady_clock::time_point last_used;
        std::string host;
        int port;
        
        PooledConnection(const std::string& h, int p) : host(h), port(p) {
            client = std::make_unique<httplib::Client>(host, port);
            client->set_connection_timeout(2, 0);
            client->set_read_timeout(5, 0);
            client->set_write_timeout(5, 0);
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
    
    std::string makeKey(const std::string& host, int port) const {
        return host + ":" + std::to_string(port);
    }
    
public:
    httplib::Client* getConnection(const std::string& host, int port) {
        std::string key = makeKey(host, port);
        
        {
            std::shared_lock<std::shared_mutex> lock(connections_mutex_);
            auto it = connections_.find(key);
            if (it != connections_.end() && !it->second->isExpired(CONNECTION_TIMEOUT_SECONDS)) {
                it->second->updateLastUsed();
                return it->second->client.get();
            }
        }
        
        {
            std::unique_lock<std::shared_mutex> lock(connections_mutex_);
            auto it = connections_.find(key);
            if (it == connections_.end() || it->second->isExpired(CONNECTION_TIMEOUT_SECONDS)) {
                connections_[key] = std::make_shared<PooledConnection>(host, port);
            }
            return connections_[key]->client.get();
        }
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