#include "grvt_auth.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <mutex>
#include <atomic>
#include <json/json.h>

namespace grvt {

// CURL global state management with reference counting
namespace {
    std::mutex curl_init_mutex;
    std::atomic<int> curl_ref_count{0};
    
    void ensure_curl_initialized() {
        std::lock_guard<std::mutex> lock(curl_init_mutex);
        if (curl_ref_count.fetch_add(1) == 0) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
        }
    }
    
    void ensure_curl_cleanup() {
        std::lock_guard<std::mutex> lock(curl_init_mutex);
        if (curl_ref_count.fetch_sub(1) == 1) {
            curl_global_cleanup();
        }
    }
}

GrvtAuth::GrvtAuth(GrvtAuthEnvironment env) : environment_(env), curl_(nullptr) {
    ensure_curl_initialized();
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cerr << "[GRVT_AUTH] Failed to initialize CURL" << std::endl;
    }
}

GrvtAuth::~GrvtAuth() {
    if (curl_) {
        curl_easy_cleanup(curl_);
    }
    ensure_curl_cleanup();
}

std::string GrvtAuth::get_base_url() const {
    switch (environment_) {
        case GrvtAuthEnvironment::PRODUCTION:
            return "https://edge.grvt.io";
        case GrvtAuthEnvironment::TESTNET:
            return "https://edge.testnet.grvt.io";
        case GrvtAuthEnvironment::STAGING:
            return "https://edge.staging.gravitymarkets.io";
        case GrvtAuthEnvironment::DEVELOPMENT:
            return "https://edge.dev.gravitymarkets.io";
        default:
            return "https://edge.grvt.io";
    }
}

std::string GrvtAuth::get_auth_endpoint() const {
    return get_base_url() + "/auth/api_key/login";
}

void GrvtAuth::set_environment(GrvtAuthEnvironment env) {
    environment_ = env;
}

GrvtAuthResult GrvtAuth::authenticate(const std::string& api_key, const std::string& sub_account_id) {
    if (api_key.empty()) {
        GrvtAuthResult result;
        result.success = false;
        result.error_message = "API key is empty";
        return result;
    }
    
    return perform_auth_request(api_key, sub_account_id);
}

GrvtAuthResult GrvtAuth::refresh_session(const std::string& api_key, const std::string& sub_account_id) {
    return authenticate(api_key, sub_account_id);
}

GrvtAuthResult GrvtAuth::perform_auth_request(const std::string& api_key, const std::string& sub_account_id) {
    GrvtAuthResult result;
    
    if (!curl_) {
        result.success = false;
        result.error_message = "CURL not initialized";
        return result;
    }
    
    std::string url = get_auth_endpoint();
    std::string response_body;
    std::string response_headers;
    
    // Build request body
    Json::Value request_body;
    request_body["api_key"] = api_key;
    if (!sub_account_id.empty()) {
        request_body["sub_account_id"] = sub_account_id;
    }
    
    Json::StreamWriterBuilder builder;
    std::string json_body = Json::writeString(builder, request_body);
    
    // Set up CURL options
    curl_easy_reset(curl_);
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_POST, 1L);
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, json_body.length());
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response_body);
    curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &response_headers);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 5L);
    
    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Cookie: rm=true;");
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
    
    // Perform request
    CURLcode res = curl_easy_perform(curl_);
    
    long response_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_slist_free_all(headers);
    
    if (res != CURLE_OK) {
        result.success = false;
        result.error_message = std::string("CURL error: ") + curl_easy_strerror(res);
        std::cerr << "[GRVT_AUTH] Authentication request failed: " << result.error_message << std::endl;
        return result;
    }
    
    if (response_code != 200 && response_code != 201) {
        result.success = false;
        result.error_message = "Authentication failed with HTTP " + std::to_string(response_code);
        std::cerr << "[GRVT_AUTH] " << result.error_message << std::endl;
        std::cerr << "[GRVT_AUTH] Response: " << response_body << std::endl;
        return result;
    }
    
    // Parse response headers to extract session cookie and account ID
    result.session_cookie = parse_session_cookie(response_headers);
    result.account_id = parse_account_id(response_headers);
    
    if (result.session_cookie.empty() || result.account_id.empty()) {
        result.success = false;
        result.error_message = "Failed to extract session cookie or account ID from response";
        std::cerr << "[GRVT_AUTH] " << result.error_message << std::endl;
        std::cerr << "[GRVT_AUTH] Headers: " << response_headers << std::endl;
        return result;
    }
    
    result.success = true;
    std::cout << "[GRVT_AUTH] Authentication successful" << std::endl;
    std::cout << "[GRVT_AUTH] Account ID: " << result.account_id << std::endl;
    std::cout << "[GRVT_AUTH] Session cookie extracted (length: " << result.session_cookie.length() << ")" << std::endl;
    
    return result;
}

std::string GrvtAuth::parse_session_cookie(const std::string& headers) {
    // Look for Set-Cookie header with gravity=...
    // Format: Set-Cookie: gravity=<cookie_value>; Path=/; ...
    std::regex cookie_regex(R"(Set-Cookie:\s*gravity=([^;]+))", std::regex_constants::icase);
    std::smatch match;
    
    if (std::regex_search(headers, match, cookie_regex)) {
        std::string cookie_value = match[1].str();
        // Return full cookie format: gravity=<value>
        return "gravity=" + cookie_value;
    }
    
    // Also try to find it in Cookie header if present
    std::regex cookie_header_regex(R"(Cookie:.*gravity=([^;]+))", std::regex_constants::icase);
    if (std::regex_search(headers, match, cookie_header_regex)) {
        std::string cookie_value = match[1].str();
        return "gravity=" + cookie_value;
    }
    
    return "";
}

std::string GrvtAuth::parse_account_id(const std::string& headers) {
    // Look for X-Grvt-Account-Id header
    // Format: X-Grvt-Account-Id: <account_id>
    std::regex account_id_regex(R"(X-Grvt-Account-Id:\s*([^\r\n]+))", std::regex_constants::icase);
    std::smatch match;
    
    if (std::regex_search(headers, match, account_id_regex)) {
        std::string account_id = match[1].str();
        // Trim whitespace
        account_id.erase(0, account_id.find_first_not_of(" \t\r\n"));
        account_id.erase(account_id.find_last_not_of(" \t\r\n") + 1);
        return account_id;
    }
    
    return "";
}

size_t GrvtAuth::HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    std::string* headers = static_cast<std::string*>(userdata);
    size_t total_size = size * nitems;
    headers->append(buffer, total_size);
    return total_size;
}

size_t GrvtAuth::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t total_size = size * nmemb;
    data->append(static_cast<char*>(contents), total_size);
    return total_size;
}

} // namespace grvt

