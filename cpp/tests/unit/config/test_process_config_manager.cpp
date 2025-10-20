#include "doctest.h"
#include "../../../utils/config/process_config_manager.hpp"
#include <fstream>
#include <iostream>

TEST_CASE("ProcessConfigManager - Load Valid Config") {
    // Create a test config file
    std::ofstream config_file("test_config.ini");
    config_file << "[test_section]\n";
    config_file << "key1 = value1\n";
    config_file << "key2 = value2\n";
    config_file << "[another_section]\n";
    config_file << "key3 = value3\n";
    config_file.close();
    
    config::ProcessConfigManager manager;
    bool loaded = manager.load_config("test_config.ini");
    
    CHECK(loaded == true);
    
    // Test reading values
    std::string value1 = manager.get_string("test_section", "key1", "");
    std::string value2 = manager.get_string("test_section", "key2", "");
    std::string value3 = manager.get_string("another_section", "key3", "");
    
    CHECK(value1 == "value1");
    CHECK(value2 == "value2");
    CHECK(value3 == "value3");
    
    // Clean up
    std::remove("test_config.ini");
}

TEST_CASE("ProcessConfigManager - Load Invalid Config") {
    config::ProcessConfigManager manager;
    bool loaded = manager.load_config("nonexistent_config.ini");
    
    CHECK(loaded == false);
}

TEST_CASE("ProcessConfigManager - Default Values") {
    config::ProcessConfigManager manager;
    
    // Test default values when config not loaded
    std::string default_value = manager.get_string("section", "key", "default");
    CHECK(default_value == "default");
    
    int default_int = manager.get_int("section", "key", 42);
    CHECK(default_int == 42);
    
    double default_double = manager.get_double("section", "key", 3.14);
    CHECK(default_double == 3.14);
}

TEST_CASE("ProcessConfigManager - Numeric Values") {
    // Create a test config file with numeric values
    std::ofstream config_file("test_numeric_config.ini");
    config_file << "[numeric_section]\n";
    config_file << "int_value = 123\n";
    config_file << "double_value = 3.14159\n";
    config_file << "bool_value = true\n";
    config_file.close();
    
    config::ProcessConfigManager manager;
    manager.load_config("test_numeric_config.ini");
    
    int int_val = manager.get_int("numeric_section", "int_value", 0);
    double double_val = manager.get_double("numeric_section", "double_value", 0.0);
    bool bool_val = manager.get_bool("numeric_section", "bool_value", false);
    
    CHECK(int_val == 123);
    CHECK(double_val == 3.14159);
    CHECK(bool_val == true);
    
    // Clean up
    std::remove("test_numeric_config.ini");
}

TEST_CASE("ProcessConfigManager - Missing Keys") {
    std::ofstream config_file("test_missing_keys.ini");
    config_file << "[section]\n";
    config_file << "existing_key = value\n";
    config_file.close();
    
    config::ProcessConfigManager manager;
    manager.load_config("test_missing_keys.ini");
    
    // Test missing keys return defaults
    std::string missing_str = manager.get_string("section", "missing_key", "default");
    int missing_int = manager.get_int("section", "missing_key", 999);
    
    CHECK(missing_str == "default");
    CHECK(missing_int == 999);
    
    // Clean up
    std::remove("test_missing_keys.ini");
}
