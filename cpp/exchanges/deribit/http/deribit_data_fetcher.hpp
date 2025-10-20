#pragma once
#include "../../i_exchange_data_fetcher.hpp"
#include "../../../proto/order.pb.h"
#include "../../../proto/position.pb.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <curl/curl.h>
#include <json/json.h>

namespace deribit {

struct DeribitConfig {
    std::string client_id;
    std::string client_secret;
    std::string access_token;
    std::string refresh_token;
    std::string base_url;
    int timeout_ms{30000};
    int max_retries{3};
};

class DeribitDataFetcher : public IExchangeDataFetcher {
public:
    DeribitDataFetcher(const std::string& client_id, const std::string& client_secret);
    ~DeribitDataFetcher();
    
    // Authentication
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;
    
    // Private data methods only
    std::vector<proto::OrderEvent> get_open_orders() override;
    std::vector<proto::PositionUpdate> get_positions() override;
    std::vector<proto::AccountBalance> get_balances() override;

private:
    DeribitConfig config_;
    CURL* curl_;
    std::atomic<bool> connected_;
    std::atomic<bool> authenticated_;
    
    // Helper methods
    std::string make_request(const std::string& method, const std::string& params = "");
    std::string authenticate();
    std::string refresh_token();
    
    // JSON parsing helpers
    std::vector<proto::OrderEvent> parse_orders(const std::string& json_response);
    std::vector<proto::PositionUpdate> parse_positions(const std::string& json_response);
    std::vector<proto::AccountBalance> parse_balances(const std::string& json_response);
    
    // CURL callback
    static size_t DataFetcherWriteCallback(void* contents, size_t size, size_t nmemb, std::string* data);
};

}