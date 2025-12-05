#pragma once
#include <string>
#include <memory>
#include <curl/curl.h>

namespace grvt {

/**
 * GRVT Authentication Result
 */
struct GrvtAuthResult {
    bool success{false};
    std::string session_cookie;  // GRVT_COOKIE (gravity=...)
    std::string account_id;      // X-Grvt-Account-Id
    std::string error_message;
    
    bool is_valid() const {
        return success && !session_cookie.empty() && !account_id.empty();
    }
};

/**
 * GRVT Authentication Environment
 */
enum class GrvtAuthEnvironment {
    PRODUCTION,
    TESTNET,
    STAGING,
    DEVELOPMENT
};

/**
 * GRVT API Key Authentication Helper
 * 
 * Implements GRVT authentication flow:
 * 1. POST to auth endpoint with API key
 * 2. Extract session cookie from Set-Cookie header
 * 3. Extract account ID from X-Grvt-Account-Id header
 * 
 * Based on: https://api-docs.grvt.io/#authentication
 */
class GrvtAuth {
public:
    GrvtAuth(GrvtAuthEnvironment env = GrvtAuthEnvironment::PRODUCTION);
    ~GrvtAuth();
    
    /**
     * Authenticate with API key and retrieve session cookie + account ID
     * 
     * @param api_key GRVT API key
     * @param sub_account_id Optional sub-account ID (for sub-account authentication)
     * @return Authentication result with session cookie and account ID
     */
    GrvtAuthResult authenticate(const std::string& api_key, const std::string& sub_account_id = "");
    
    /**
     * Refresh session cookie (re-authenticate)
     * 
     * @param api_key GRVT API key
     * @return Updated authentication result
     */
    GrvtAuthResult refresh_session(const std::string& api_key, const std::string& sub_account_id = "");
    
    /**
     * Get authentication endpoint URL for current environment
     */
    std::string get_auth_endpoint() const;
    
    /**
     * Set authentication environment
     */
    void set_environment(GrvtAuthEnvironment env);

private:
    GrvtAuthEnvironment environment_;
    CURL* curl_;
    
    /**
     * Perform HTTP authentication request
     */
    GrvtAuthResult perform_auth_request(const std::string& api_key, const std::string& sub_account_id);
    
    /**
     * Parse Set-Cookie header to extract session cookie
     */
    std::string parse_session_cookie(const std::string& set_cookie_header);
    
    /**
     * Parse response headers to extract account ID
     */
    std::string parse_account_id(const std::string& headers);
    
    /**
     * CURL write callback for headers
     */
    static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    
    /**
     * CURL write callback for response body
     */
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data);
    
    /**
     * Get base URL for environment
     */
    std::string get_base_url() const;
};

} // namespace grvt

