CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O2
INCLUDES = -Iinclude
LIBS = -pthread

# Source directories
SRC_DIR = src
APPS_DIR = apps/example
BUILD_DIR = build

# Find all source files
SERVER_SOURCES = $(SRC_DIR)/server/tribune_server.cpp $(SRC_DIR)/server/client_state.cpp $(SRC_DIR)/protocol/parser.cpp $(SRC_DIR)/mpc/sum_computation.cpp $(SRC_DIR)/crypto/signature.cpp
CLIENT_SOURCES = $(SRC_DIR)/client/tribune_client.cpp $(SRC_DIR)/client/data_collection_module.cpp $(SRC_DIR)/protocol/parser.cpp $(SRC_DIR)/mpc/sum_computation.cpp $(SRC_DIR)/crypto/signature.cpp

# External dependencies (assuming they're installed or will be handled by CMake)
HTTPLIB_FLAGS = -DCPPHTTPLIB_OPENSSL_SUPPORT
JSON_FLAGS = 

# Default target
all: server_app client_app

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Server application
server_app: $(BUILD_DIR) $(SERVER_SOURCES) $(APPS_DIR)/server_app.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(HTTPLIB_FLAGS) $(JSON_FLAGS) \
		$(APPS_DIR)/server_app.cpp $(SERVER_SOURCES) \
		-o server_app $(LIBS)

# Client application  
client_app: $(BUILD_DIR) $(CLIENT_SOURCES) $(APPS_DIR)/client_app.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(HTTPLIB_FLAGS) $(JSON_FLAGS) \
		$(APPS_DIR)/client_app.cpp $(CLIENT_SOURCES) \
		-o client_app $(LIBS)

# Run demo (requires Python 3)
demo: all
	@echo "Building applications..."
	@echo "Starting MPC demo..."
	python3 scripts/run_demo.py

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f server_app client_app

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y build-essential cmake python3 openssl libssl-dev

.PHONY: all clean demo install-deps