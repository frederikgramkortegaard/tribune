#pragma once
#include "events/events.hpp"
#include <optional>
#include <string>

std::optional<EventResponse> parseSubmitResponse(const std::string &body);
std::optional<ConnectResponse> parseConnectResponse(const std::string &body);
