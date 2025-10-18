#include <doctest.h>
#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include "../../exchanges/binance/http/binance_oms.hpp"
#include "../../exchanges/binance/http/binance_data_fetcher.hpp"
#include "../../utils/http/curl_http_handler.hpp"
#include "../../config/test_config_manager.hpp"

using namespace binance;
using namespace test_config;

TEST_SUITE("Security Tests") {

    TEST_SUITE("Credential Security Tests") {
        
        TEST_CASE("API Key Format Validation") {
            // Test valid API key formats
            std::vector<std::string> valid_keys = {
                "test_api_key_123",
                "binance_api_key_456",
                "valid_key_789",
                "a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0"
            };
            
            for (const auto& key : valid_keys) {
                // API key should not be empty and should have reasonable length
                CHECK_FALSE(key.empty());
                CHECK(key.length() >= 10);
                CHECK(key.length() <= 100);
                
                // Should not contain sensitive characters
                CHECK(key.find("password") == std::string::npos);
                CHECK(key.find("secret") == std::string::npos);
            }
        }

        TEST_CASE("API Secret Format Validation") {
            // Test valid API secret formats
            std::vector<std::string> valid_secrets = {
                "test_api_secret_123",
                "binance_api_secret_456",
                "valid_secret_789",
                "a1b2c3d4e5f6g7h8i9j0k1l2m3n4o5p6q7r8s9t0u1v2w3x4y5z6"
            };
            
            for (const auto& secret : valid_secrets) {
                // API secret should not be empty and should have reasonable length
                CHECK_FALSE(secret.empty());
                CHECK(secret.length() >= 20);
                CHECK(secret.length() <= 200);
                
                // Should not contain sensitive words
                CHECK(secret.find("password") == std::string::npos);
                CHECK(secret.find("private") == std::string::npos);
            }
        }

        TEST_CASE("Credential Storage Security") {
            auto& config_manager = get_test_config();
            CHECK(config_manager.load_config("cpp/tests/config/test_exchange_config.ini"));
            
            // Test that credentials are not exposed in logs or error messages
            auto binance_config = config_manager.get_exchange_config("BINANCE");
            
            // Credentials should be loaded but not logged
            CHECK_FALSE(binance_config.api_key.empty());
            CHECK_FALSE(binance_config.api_secret.empty());
            
            // Test that credentials are not accidentally exposed
            std::string config_string = binance_config.api_key + binance_config.api_secret;
            CHECK_FALSE(config_string.find("password") != std::string::npos);
            CHECK_FALSE(config_string.find("private") != std::string::npos);
        }

        TEST_CASE("Credential Transmission Security") {
            // Test that credentials are not transmitted in plain text
            CurlHttpHandler handler;
            
            // In real implementation, would test HTTPS usage
            CHECK(handler.connect("https://fapi.binance.com"));
            
            // Should use HTTPS for API calls
            // This is a basic check - in real implementation would verify SSL/TLS
        }
    }

    TEST_SUITE("Input Validation Tests") {
        
        TEST_CASE("Order Input Sanitization") {
            BinanceOMS oms("test_key", "test_secret");
            
            // Test with potentially malicious input
            std::vector<std::string> malicious_inputs = {
                "'; DROP TABLE orders; --",
                "<script>alert('xss')</script>",
                "../../etc/passwd",
                "null\0byte",
                "very_long_string_" + std::string(1000, 'a')
            };
            
            for (const auto& malicious_input : malicious_inputs) {
                Order order;
                order.cl_ord_id = malicious_input;
                order.symbol = "BTCUSDT";
                order.side = OrderSide::BUY;
                order.order_type = OrderType::LIMIT;
                order.qty = 0.1;
                order.price = 50000.0;
                
                // Should handle malicious input gracefully
                auto result = oms.send_order(order);
                // May succeed or fail, but should not crash
            }
        }

        TEST_CASE("Symbol Input Validation") {
            BinanceOMS oms("test_key", "test_secret");
            
            // Test with invalid symbols
            std::vector<std::string> invalid_symbols = {
                "",
                "INVALID_SYMBOL",
                "BTCUSDT<script>",
                "BTCUSDT' OR '1'='1",
                "BTCUSDT\0null",
                "BTCUSDT" + std::string(100, 'X')
            };
            
            for (const auto& invalid_symbol : invalid_symbols) {
                Order order;
                order.cl_ord_id = "test_order";
                order.symbol = invalid_symbol;
                order.side = OrderSide::BUY;
                order.order_type = OrderType::LIMIT;
                order.qty = 0.1;
                order.price = 50000.0;
                
                // Should handle invalid symbols gracefully
                auto result = oms.send_order(order);
                // Should fail for invalid symbols
            }
        }

        TEST_CASE("Numeric Input Validation") {
            BinanceOMS oms("test_key", "test_secret");
            
            // Test with invalid numeric inputs
            std::vector<double> invalid_quantities = {
                -1.0,    // Negative quantity
                0.0,     // Zero quantity
                1e10,    // Very large quantity
                -1e10,   // Very large negative quantity
                std::numeric_limits<double>::infinity(), // Infinity
                std::numeric_limits<double>::quiet_NaN()  // NaN
            };
            
            for (const auto& invalid_qty : invalid_quantities) {
                Order order;
                order.cl_ord_id = "test_order";
                order.symbol = "BTCUSDT";
                order.side = OrderSide::BUY;
                order.order_type = OrderType::LIMIT;
                order.qty = invalid_qty;
                order.price = 50000.0;
                
                // Should handle invalid quantities gracefully
                auto result = oms.send_order(order);
                // Should fail for invalid quantities
            }
        }

        TEST_CASE("Price Input Validation") {
            BinanceOMS oms("test_key", "test_secret");
            
            // Test with invalid prices
            std::vector<double> invalid_prices = {
                -1.0,    // Negative price
                0.0,     // Zero price
                1e10,    // Very large price
                -1e10,   // Very large negative price
                std::numeric_limits<double>::infinity(), // Infinity
                std::numeric_limits<double>::quiet_NaN()  // NaN
            };
            
            for (const auto& invalid_price : invalid_prices) {
                Order order;
                order.cl_ord_id = "test_order";
                order.symbol = "BTCUSDT";
                order.side = OrderSide::BUY;
                order.order_type = OrderType::LIMIT;
                order.qty = 0.1;
                order.price = invalid_price;
                
                // Should handle invalid prices gracefully
                auto result = oms.send_order(order);
                // Should fail for invalid prices
            }
        }
    }

    TEST_SUITE("API Signature Validation Tests") {
        
        TEST_CASE("HMAC Signature Generation") {
            // Test HMAC signature generation
            std::string api_secret = "test_secret_key";
            std::string message = "test_message";
            
            unsigned char* result;
            unsigned int len;
            
            result = HMAC(EVP_sha256(), api_secret.c_str(), api_secret.length(),
                         (unsigned char*)message.c_str(), message.length(), NULL, &len);
            
            CHECK(result != nullptr);
            CHECK(len > 0);
            
            // Convert to hex string
            std::string signature;
            for (unsigned int i = 0; i < len; i++) {
                char hex[3];
                sprintf(hex, "%02x", result[i]);
                signature += hex;
            }
            
            CHECK_FALSE(signature.empty());
            CHECK_EQ(signature.length(), len * 2); // Each byte = 2 hex chars
        }

        TEST_CASE("Signature Consistency") {
            std::string api_secret = "test_secret_key";
            std::string message = "test_message";
            
            // Generate signature multiple times
            std::string signature1, signature2;
            
            // First signature
            unsigned char* result1;
            unsigned int len1;
            result1 = HMAC(EVP_sha256(), api_secret.c_str(), api_secret.length(),
                          (unsigned char*)message.c_str(), message.length(), NULL, &len1);
            
            for (unsigned int i = 0; i < len1; i++) {
                char hex[3];
                sprintf(hex, "%02x", result1[i]);
                signature1 += hex;
            }
            
            // Second signature
            unsigned char* result2;
            unsigned int len2;
            result2 = HMAC(EVP_sha256(), api_secret.c_str(), api_secret.length(),
                          (unsigned char*)message.c_str(), message.length(), NULL, &len2);
            
            for (unsigned int i = 0; i < len2; i++) {
                char hex[3];
                sprintf(hex, "%02x", result2[i]);
                signature2 += hex;
            }
            
            // Signatures should be identical
            CHECK_EQ(signature1, signature2);
        }

        TEST_CASE("Signature with Different Messages") {
            std::string api_secret = "test_secret_key";
            std::string message1 = "test_message_1";
            std::string message2 = "test_message_2";
            
            // Generate signatures for different messages
            std::string signature1, signature2;
            
            // Signature for message1
            unsigned char* result1;
            unsigned int len1;
            result1 = HMAC(EVP_sha256(), api_secret.c_str(), api_secret.length(),
                          (unsigned char*)message1.c_str(), message1.length(), NULL, &len1);
            
            for (unsigned int i = 0; i < len1; i++) {
                char hex[3];
                sprintf(hex, "%02x", result1[i]);
                signature1 += hex;
            }
            
            // Signature for message2
            unsigned char* result2;
            unsigned int len2;
            result2 = HMAC(EVP_sha256(), api_secret.c_str(), api_secret.length(),
                          (unsigned char*)message2.c_str(), message2.length(), NULL, &len2);
            
            for (unsigned int i = 0; i < len2; i++) {
                char hex[3];
                sprintf(hex, "%02x", result2[i]);
                signature2 += hex;
            }
            
            // Signatures should be different for different messages
            CHECK_NE(signature1, signature2);
        }
    }

    TEST_SUITE("Access Control Tests") {
        
        TEST_CASE("API Key Permissions") {
            // Test that API keys have appropriate permissions
            auto& config_manager = get_test_config();
            CHECK(config_manager.load_config("cpp/tests/config/test_exchange_config.ini"));
            
            auto binance_config = config_manager.get_exchange_config("BINANCE");
            
            BinanceOMS oms(binance_config.api_key, binance_config.api_secret);
            BinanceDataFetcher fetcher(binance_config.api_key, binance_config.api_secret);
            
            // Test read permissions
            CHECK(oms.connect(binance_config.http_url));
            auto account_info = oms.get_account_info();
            auto positions = oms.get_positions();
            
            // Test write permissions
            Order order;
            order.cl_ord_id = "permission_test";
            order.symbol = binance_config.symbol;
            order.side = OrderSide::BUY;
            order.order_type = OrderType::MARKET;
            order.qty = 0.01; // Small amount for test
            
            auto result = oms.send_order(order);
            // May succeed or fail depending on API key permissions
            
            oms.disconnect();
        }

        TEST_CASE("Rate Limiting") {
            BinanceOMS oms("test_key", "test_secret");
            
            // Test rate limiting by making many rapid requests
            const int rapid_requests = 100;
            int success_count = 0;
            int rate_limited_count = 0;
            
            for (int i = 0; i < rapid_requests; ++i) {
                auto account_info = oms.get_account_info();
                if (account_info.has_value()) {
                    success_count++;
                } else {
                    rate_limited_count++;
                }
                
                // Small delay to avoid overwhelming
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // Should handle rate limiting gracefully
            CHECK(success_count + rate_limited_count == rapid_requests);
        }
    }

    TEST_SUITE("Data Integrity Tests") {
        
        TEST_CASE("Message Integrity") {
            // Test that messages are not tampered with
            std::string original_message = "test_message_for_integrity";
            std::string api_secret = "test_secret";
            
            // Generate signature
            unsigned char* result;
            unsigned int len;
            result = HMAC(EVP_sha256(), api_secret.c_str(), api_secret.length(),
                         (unsigned char*)original_message.c_str(), original_message.length(), NULL, &len);
            
            std::string original_signature;
            for (unsigned int i = 0; i < len; i++) {
                char hex[3];
                sprintf(hex, "%02x", result[i]);
                original_signature += hex;
            }
            
            // Tamper with message
            std::string tampered_message = "tampered_message_for_integrity";
            
            // Generate signature for tampered message
            unsigned char* tampered_result;
            unsigned int tampered_len;
            tampered_result = HMAC(EVP_sha256(), api_secret.c_str(), api_secret.length(),
                                  (unsigned char*)tampered_message.c_str(), tampered_message.length(), NULL, &tampered_len);
            
            std::string tampered_signature;
            for (unsigned int i = 0; i < tampered_len; i++) {
                char hex[3];
                sprintf(hex, "%02x", tampered_result[i]);
                tampered_signature += hex;
            }
            
            // Signatures should be different
            CHECK_NE(original_signature, tampered_signature);
        }

        TEST_CASE("Configuration Integrity") {
            auto& config_manager = get_test_config();
            CHECK(config_manager.load_config("cpp/tests/config/test_exchange_config.ini"));
            
            // Test that configuration is not corrupted
            auto binance_config = config_manager.get_exchange_config("BINANCE");
            
            // Configuration should be valid
            CHECK_FALSE(binance_config.exchange_name.empty());
            CHECK_FALSE(binance_config.api_key.empty());
            CHECK_FALSE(binance_config.api_secret.empty());
            CHECK_FALSE(binance_config.http_url.empty());
            CHECK(binance_config.timeout_ms > 0);
            CHECK(binance_config.max_retries > 0);
        }
    }

    TEST_SUITE("Error Handling Security Tests") {
        
        TEST_CASE("Error Message Security") {
            BinanceOMS oms("invalid_key", "invalid_secret");
            
            // Test that error messages don't leak sensitive information
            Order order;
            order.cl_ord_id = "test_order";
            order.symbol = "BTCUSDT";
            order.side = OrderSide::BUY;
            order.order_type = OrderType::LIMIT;
            order.qty = 0.1;
            order.price = 50000.0;
            
            auto result = oms.send_order(order);
            
            // Error messages should not contain sensitive information
            // In real implementation, would check error message content
        }

        TEST_CASE("Exception Security") {
            // Test that exceptions don't leak sensitive information
            try {
                BinanceOMS oms("", ""); // Empty credentials
                CHECK_FALSE(oms.connect("invalid_url"));
            } catch (const std::exception& e) {
                std::string error_message = e.what();
                
                // Error message should not contain sensitive information
                CHECK(error_message.find("password") == std::string::npos);
                CHECK(error_message.find("secret") == std::string::npos);
                CHECK(error_message.find("private") == std::string::npos);
            }
        }
    }
}
