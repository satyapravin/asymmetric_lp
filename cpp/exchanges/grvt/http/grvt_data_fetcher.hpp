#pragma once
#include "../../i_exchange_data_fetcher.hpp"
#include "../../../proto/order.pb.h"
#include "../../../proto/position.pb.h"
#include "../../../proto/market_data.pb.h"
#include "../grvt_auth.hpp"
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
    // Constructor with pre-authenticated credentials
    GrvtDataFetcher(const std::string& api_key, const std::string& session_cookie, const std::string& account_id);
    
    // Constructor with API key only (will authenticate automatically)
    explicit GrvtDataFetcher(const std::string& api_key, GrvtAuthEnvironment env = GrvtAuthEnvironment::PRODUCTION);
    
    ~GrvtDataFetcher();
    
    // Authentication
    void set_auth_credentials(const std::string& api_key, const std::string& secret) override;
    bool is_authenticated() const override;
    
    // GRVT-specific: Authenticate with API key to get session cookie and account ID
    bool authenticate_with_api_key(const std::string& api_key, const std::string& sub_account_id = "");
    
    // Private data methods only
    std::vector<proto::OrderEvent> get_open_orders() override;
    std::vector<proto::PositionUpdate> get_positions() override;
    std::vector<proto::AccountBalance> get_balances() override;

private:
    GrvtConfig config_;
    CURL* curl_;
    std::atomic<bool> connected_;
    std::atomic<bool> authenticated_;
    std::unique_ptr<GrvtAuth> auth_helper_;
    GrvtAuthEnvironment auth_environment_;
    
    // Helper methods
    std::string make_request(const std::string& method, const std::string& params = "");
    std::string create_auth_headers();
    
    // JSON parsing helpers
    std::vector<proto::OrderEvent> parse_orders(const std::string& json_response);
    std::vector<proto::PositionUpdate> parse_positions(const std::string& json_response);
    std::vector<proto::AccountBalance> parse_balances(const std::string& json_response);
    
    // CURL callback
    static size_t DataFetcherWriteCallback(void* contents, size_t size, size_t nmemb, std::string* data);
};

}