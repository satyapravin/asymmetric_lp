#pragma once
#include "logger.hpp"
#include <sstream>

/**
 * Logging helper macros for convenient logging
 * Usage: LOG_INFO("Component", "message") or LOG_ERROR("Component", "message")
 */
#define LOG_INFO_COMP(component, msg) \
    do { \
        logging::Logger logger(component); \
        logger.info(msg); \
    } while(0)

#define LOG_WARN_COMP(component, msg) \
    do { \
        logging::Logger logger(component); \
        logger.warn(msg); \
    } while(0)

#define LOG_ERROR_COMP(component, msg) \
    do { \
        logging::Logger logger(component); \
        logger.error(msg); \
    } while(0)

#define LOG_DEBUG_COMP(component, msg) \
    do { \
        logging::Logger logger(component); \
        logger.debug(msg); \
    } while(0)

/**
 * Helper function to convert stream to string
 */
template<typename T>
std::string to_string(const T& value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
}

