#include "server/client_state.hpp"

void ClientState::markReceivedEvent() {
    this->eventsSinceLastClientResponse_ = 0;
}

bool ClientState::isClientParticipating() const {
    //@TODO : This should probably be more complex.
    // For now, always return true so all connected clients can participate
    return true;
    // Original logic: return this->eventsSinceLastClientResponse < 5;
}