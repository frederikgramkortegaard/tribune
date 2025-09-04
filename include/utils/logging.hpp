#pragma once

#include <iostream>
#include <cstdlib>

// Always-on logging for important messages (works in release too)
#define LOG(msg) do { \
  std::cout << "[LOG] " << msg << std::endl; \
} while(0)

// Debug logging levels
#ifdef DEBUG_BUILD
  #define DEBUG_INFO(msg) do { \
    std::cout << "[INFO] " << msg << std::endl; \
  } while(0)
  
  #define DEBUG_DEBUG(msg) do { \
    std::cout << "[DEBUG] " << msg << std::endl; \
  } while(0)
  
  #define DEBUG_WARN(msg) do { \
    std::cerr << "[WARN] " << msg << std::endl; \
  } while(0)
  
  #define DEBUG_ERROR(msg) do { \
    std::cerr << "[ERROR] " << msg << std::endl; \
  } while(0)
#else
  // All debug macros become no-ops in release
  #define DEBUG_INFO(msg) ((void)0)
  #define DEBUG_DEBUG(msg) ((void)0)
  #define DEBUG_WARN(msg) ((void)0)
  #define DEBUG_ERROR(msg) ((void)0)
#endif

// Always log errors and exit (even in release)
#define LOG_AND_EXIT(msg, code) do { \
  std::cerr << "[FATAL] " << msg << std::endl; \
  std::exit(code); \
} while(0)