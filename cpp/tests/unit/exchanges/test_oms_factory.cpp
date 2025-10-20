#include "doctest.h"
#include "../../../exchanges/oms_factory.hpp"
#include "../../../exchanges/i_exchange_oms.hpp"

TEST_CASE("OMSFactory - Create Binance OMS") {
    auto oms = exchanges::OMSFactory::create("binance");
    
    CHECK(oms != nullptr);
    CHECK(oms->get_exchange_name() == "binance");
}

TEST_CASE("OMSFactory - Create Deribit OMS") {
    auto oms = exchanges::OMSFactory::create("deribit");
    
    CHECK(oms != nullptr);
    CHECK(oms->get_exchange_name() == "deribit");
}

TEST_CASE("OMSFactory - Create Grvt OMS") {
    auto oms = exchanges::OMSFactory::create("grvt");
    
    CHECK(oms != nullptr);
    CHECK(oms->get_exchange_name() == "grvt");
}

TEST_CASE("OMSFactory - Case Insensitive Exchange Names") {
    auto oms1 = exchanges::OMSFactory::create("BINANCE");
    auto oms2 = exchanges::OMSFactory::create("Binance");
    auto oms3 = exchanges::OMSFactory::create("binance");
    
    CHECK(oms1 != nullptr);
    CHECK(oms2 != nullptr);
    CHECK(oms3 != nullptr);
    
    CHECK(oms1->get_exchange_name() == "binance");
    CHECK(oms2->get_exchange_name() == "binance");
    CHECK(oms3->get_exchange_name() == "binance");
}

TEST_CASE("OMSFactory - Unknown Exchange") {
    auto oms = exchanges::OMSFactory::create("unknown_exchange");
    
    CHECK(oms == nullptr);
}

TEST_CASE("OMSFactory - Empty Exchange Name") {
    auto oms = exchanges::OMSFactory::create("");
    
    CHECK(oms == nullptr);
}

TEST_CASE("OMSFactory - OMS Interface Methods") {
    auto oms = exchanges::OMSFactory::create("binance");
    
    CHECK(oms != nullptr);
    
    // Test that OMS implements required interface methods
    // These should not throw exceptions
    CHECK_NOTHROW(oms->connect());
    CHECK_NOTHROW(oms->disconnect());
    CHECK_NOTHROW(oms->is_connected());
}
