#include "server/client_state.hpp"

bool ClientState::isAlive(int timeout_seconds) const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_ping_time_);
    return elapsed.count() < timeout_seconds;
}

void ClientState::updatePingTime() {
    last_ping_time_ = std::chrono::steady_clock::now();
}