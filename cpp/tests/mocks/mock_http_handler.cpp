#include "mock_http_handler.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <random>
#include <chrono>
#include <thread>

MockHttpHandler::MockHttpHandler(const std::string& test_data_dir) 
    : test_data_dir_(test_data_dir), rng_(std::random_device{}()) {
}

HttpResponse MockHttpHandler::make_request(const HttpRequest& request) {
    // Simulate network failure
    if (network_failure_enabled_) {
        HttpResponse response;
        response.status_code = 0;
        response.error_message = "Network failure simulation";
        response.success = false;
        return response;
    }
    
    // Simulate random failures
    if (should_fail()) {
        HttpResponse response;
        response.status_code = 500;
        response.error_message = "Random failure simulation";
        response.success = false;
        return response;
    }
    
    // Simulate response delay
    if (response_delay_.count() > 0) {
        std::this_thread::sleep_for(response_delay_);
    }
    
    // Get response file path based on request
    std::string file_path = get_response_file_path(request);
    
    // Load response from file
    HttpResponse response = load_response_from_file(file_path);
    
    // If file not found, return 404
    if (response.status_code == 0) {
        response.status_code = 404;
        response.error_message = "Mock response file not found: " + file_path;
        response.success = false;
    }
    
    return response;
}

void MockHttpHandler::make_request_async(const HttpRequest& request, HttpResponseCallback callback) {
    // For testing, implement as synchronous call in a separate thread
    std::thread([this, request, callback]() {
        HttpResponse response = make_request(request);
        callback(response);
    }).detach();
}

std::string MockHttpHandler::get_response_file_path(const HttpRequest& request) {
    // Extract endpoint from URL
    std::string url = request.url;
    size_t query_pos = url.find('?');
    if (query_pos != std::string::npos) {
        url = url.substr(0, query_pos);
    }
    
    // Map endpoints to response files
    std::string filename;
    if (url.find("/fapi/v2/balance") != std::string::npos) {
        filename = "balance_response.json";
    } else if (url.find("/fapi/v2/positionRisk") != std::string::npos) {
        filename = "position_risk_response.json";
    } else if (url.find("/fapi/v2/openOrders") != std::string::npos) {
        filename = "open_orders_response.json";
    } else if (url.find("/fapi/v2/account") != std::string::npos) {
        filename = "account_response.json";
    } else if (url.find("/api/v1/sub_account_summary") != std::string::npos) {
        filename = "sub_account_summary_response.json";
    } else if (url.find("/api/v1/positions") != std::string::npos) {
        filename = "positions_response.json";
    } else if (url.find("/api/v1/orders") != std::string::npos) {
        filename = "orders_response.json";
    } else if (url.find("/api/v2/private/get_account_summary") != std::string::npos) {
        filename = "account_summary_response.json";
    } else if (url.find("/api/v2/private/get_positions") != std::string::npos) {
        filename = "positions_response.json";
    } else if (url.find("/api/v2/private/get_open_orders_by_currency") != std::string::npos) {
        filename = "open_orders_response.json";
    } else {
        filename = "auth_error_response.json";
    }
    
    return test_data_dir_ + "/" + filename;
}

HttpResponse MockHttpHandler::load_response_from_file(const std::string& file_path) {
    HttpResponse response;
    
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return response; // Return empty response (status_code = 0)
    }
    
    // Read entire file content
    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    file.close();
    
    response.body = content;
    response.status_code = 200;
    response.success = true;
    
    return response;
}

HttpResponse MockHttpHandler::create_error_response(int status_code, const std::string& error_message) {
    HttpResponse response;
    response.status_code = status_code;
    response.error_message = error_message;
    response.success = false;
    return response;
}

bool MockHttpHandler::should_fail() const {
    if (failure_rate_ <= 0.0) return false;
    
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(rng_) < failure_rate_;
}
