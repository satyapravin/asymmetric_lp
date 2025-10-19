#pragma once
#include "../../i_exchange_data_fetcher.hpp"
#include "../../../proto/order.pb.h"
#include "../../../proto/position.pb.h"
#include "../../../proto/market_data.pb.h"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <curl/curl.h>
#include <json/json.h>

namespace grvt {

struct GrvtConfig {
    std::string api_key;
    std::string session_cookie;
    std::string account_id;
    std::string base_url;
    int timeout_ms{30000};
    int max_retries{3};
};

class GrvtDataFetcher : public IExchangeDataFetcher {
public:
    GrvtDataFetcher(const std::string& api_key, const std::string& session_cookie, const std::string& account_id);
    ~GrvtDataFetcher();
    
    // Authentication
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;
    
    // Private data methods only
    std::vector<proto::OrderEvent> get_open_orders() override;
    std::vector<proto::OrderEvent> get_order_history() override;
    std::vector<proto::PositionUpdate> get_positions() override;

private:
    GrvtConfig config_;
    CURL* curl_;
    std::atomic<bool> connected_;
    std::atomic<bool> authenticated_;
    
    // Helper methods
    std::string make_request(const std::string& method, const std::string& params = "");
    std::string create_auth_headers();
    
    // JSON parsing helpers
    std::vector<proto::OrderEvent> parse_orders(const std::string& json_response);
    std::vector<proto::PositionUpdate> parse_positions(const std::string& json_response);
    
    // CURL callback
    static size_t DataFetcherWriteCallback(void* contents, size_t size, size_t nmemb, std::string* data);
};

}