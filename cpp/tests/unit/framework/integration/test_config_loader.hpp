#pragma once
#include <string>
#include <map>
#include <fstream>
#include <json/json.h>

namespace test_config {

/**
 * Configuration loader for test environments
 * Loads credentials from config files and environment variables
 */
class TestConfigLoader {
private:
    std::map<std::string, std::string> credentials_;
    std::string config_file_path_;

public:
    TestConfigLoader(const std::string& config_file_path) 
        : config_file_path_(config_file_path) {}

    /**
     * Load testnet credentials from config file
     */
    bool load_testnet_credentials() {
        std::ifstream config_file(config_file_path_);
        if (!config_file.is_open()) {
            return false;
        }

        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;

        if (!Json::parseFromStream(builder, config_file, &root, &errors)) {
            return false;
        }

        // Extract testnet credentials
        if (root.isMember("authentication")) {
            const Json::Value& auth = root["authentication"];
            
            if (auth.isMember("testnet_api_key")) {
                credentials_["api_key"] = auth["testnet_api_key"].asString();
            }
            
            if (auth.isMember("testnet_secret")) {
                credentials_["secret"] = auth["testnet_secret"].asString();
            }
        }

        return !credentials_.empty();
    }

    /**
     * Load credentials from environment variables
     */
    bool load_from_environment() {
        const char* api_key = std::getenv("BINANCE_TESTNET_API_KEY");
        const char* secret = std::getenv("BINANCE_TESTNET_SECRET");

        if (api_key && secret) {
            credentials_["api_key"] = std::string(api_key);
            credentials_["secret"] = std::string(secret);
            return true;
        }

        return false;
    }

    /**
     * Get API key
     */
    std::string get_api_key() const {
        auto it = credentials_.find("api_key");
        return (it != credentials_.end()) ? it->second : "";
    }

    /**
     * Get secret
     */
    std::string get_secret() const {
        auto it = credentials_.find("secret");
        return (it != credentials_.end()) ? it->second : "";
    }

    /**
     * Check if credentials are loaded
     */
    bool has_credentials() const {
        return !credentials_.empty() && 
               credentials_.find("api_key") != credentials_.end() &&
               credentials_.find("secret") != credentials_.end() &&
               !credentials_.at("api_key").empty() && 
               !credentials_.at("secret").empty();
    }

    /**
     * Get test symbol (BTCUSDT for Binance testnet)
     */
    std::string get_test_symbol() const {
        return "BTCUSDT";
    }
};

} // namespace test_config
