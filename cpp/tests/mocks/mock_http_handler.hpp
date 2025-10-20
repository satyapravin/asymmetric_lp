#pragma once
#include "../../utils/http/i_http_handler.hpp"
#include <fstream>
#include <filesystem>
#include <random>
#include <chrono>

class MockHttpHandler : public IHttpHandler {
public:
    MockHttpHandler(const std::string& test_data_dir);
    
    // IHttpHandler interface
    HttpResponse make_request(const HttpRequest& request) override;
    void make_request_async(const HttpRequest& request, HttpResponseCallback callback) override;
    
    bool initialize() override { return true; }
    void shutdown() override {}
    bool is_initialized() const override { return true; }
    
    void set_default_timeout(int timeout_ms) override {}
    void set_default_headers(const std::map<std::string, std::string>& headers) override {}
    void set_verify_ssl(bool verify) override {}
    
    // Test configuration
    void set_response_delay(std::chrono::milliseconds delay) { response_delay_ = delay; }
    void set_failure_rate(double rate) { failure_rate_ = rate; }
    void enable_network_failure(bool enable) { network_failure_enabled_ = enable; }
    
private:
    std::string test_data_dir_;
    std::chrono::milliseconds response_delay_{0};
    double failure_rate_{0.0};
    bool network_failure_enabled_{false};
    std::mt19937 rng_;
    
    std::string get_response_file_path(const HttpRequest& request);
    HttpResponse load_response_from_file(const std::string& file_path);
    HttpResponse create_error_response(int status_code, const std::string& error_message);
    bool should_fail() const;
};
