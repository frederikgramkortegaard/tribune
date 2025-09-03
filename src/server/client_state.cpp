#include "server/client_state.hpp"

void ClientState::markReceivedEvent() {
    this->eventsSinceLastClientResponse = 0;
}

bool ClientState::isClientParticipating() const {
    //@TODO : This should probably be more complex.
    return this->eventsSinceLastClientResponse < 5;
}