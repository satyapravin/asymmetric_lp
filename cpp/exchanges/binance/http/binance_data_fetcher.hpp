#pragma once
#include "../../i_exchange_data_fetcher.hpp"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <curl/curl.h>

namespace binance {

class BinanceDataFetcher : public IExchangeDataFetcher {
public:
    BinanceDataFetcher(const std::string& api_key, const std::string& api_secret);
    ~BinanceDataFetcher();
    
    // Authentication
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;
    
    // Private data methods only
    std::vector<proto::OrderEvent> get_open_orders() override;
    std::vector<proto::PositionUpdate> get_positions() override;
    std::vector<proto::AccountBalance> get_balances() override;

private:
    std::string api_key_;
    std::string api_secret_;
    std::string base_url_;
    CURL* curl_;
    std::atomic<bool> authenticated_;
    
    // Helper methods
    std::string make_request(const std::string& endpoint, const std::string& params = "");
    std::string create_signature(const std::string& query_string);
    std::string get_timestamp();
    
    // JSON parsing helpers
    std::vector<proto::OrderEvent> parse_orders(const std::string& json_response);
    std::vector<proto::PositionUpdate> parse_positions(const std::string& json_response);
    std::vector<proto::AccountBalance> parse_balances(const std::string& json_response);
    
    // CURL callback
    static size_t DataFetcherWriteCallback(void* contents, size_t size, size_t nmemb, std::string* data);
};

}