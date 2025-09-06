#pragma once
#include "events/events.hpp"
#include <string>
#include <vector>

// Abstract interface for data collection modules
class DataCollectionModule {
public:
  virtual ~DataCollectionModule() = default;

  // Called when client receives an event and needs to provide data
  // The event contains computation_metadata that can guide data collection
  // Returns the raw data value that will be secret-shared by the MPC protocol
  virtual std::string collectData(const Event &event) = 0;
};
