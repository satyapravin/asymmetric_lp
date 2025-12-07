#pragma once
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>
#include <map>

struct MessageHandlerConfig {
  std::string name;
  std::string endpoint;
  std::string topic;
  bool enabled{true};
};

struct ConfigSection {
  std::string name;
  std::vector<std::pair<std::string, std::string>> entries;
};

struct AppConfig {
  std::string zmq_endpoint;
  std::string zmq_topic;
  std::string exchanges_csv;
  std::string symbol;
  std::string mm_exch; // exchange to run MM on
  double min_order_qty{0.0};
  double max_order_qty{0.0};
  int poll_sleep_ms{0};
  
  // Message handler configurations
  std::vector<MessageHandlerConfig> message_handlers;
  
  // External MD/ORD/POS buses
  std::string md_pub_endpoint;  // where quote_server binds PUB
  std::string md_sub_endpoint;  // where MM connects SUB
  std::string md_topic;         // optional override; default md.{exch}.{symbol}
  std::string ord_pub_endpoint; // where exec_handler binds PUB (events)
  std::string ord_sub_endpoint; // where MM connects SUB for events OR exec connects SUB for orders
  std::string ord_topic_new;    // topic to publish new orders
  std::string ord_topic_ev;     // topic to publish order events
  std::string pos_pub_endpoint; // where position_server binds PUB
  std::string pos_sub_endpoint; // where MM connects SUB for positions
  std::string pos_topic;        // optional override; default pos.{exch}.{symbol}

  // Quote server specific
  std::string websocket_url;    // required for quote_server
  double publish_rate_hz{0.0};  // optional; default set in code if <= 0
  int max_depth{0};             // optional; default set in code if <= 0
  std::string parser;           // optional; defaults to EXCHANGES if empty
  bool snapshot_only{false};    // prefer snapshot books over incremental
  int book_depth{0};            // preferred snapshot depth (e.g., 20/25/50/100)

  // Arbitrary sections for per-process/per-exchange config
  std::vector<ConfigSection> sections;
};

inline std::string getenv_or(const char* key, const char* defv) {
  const char* v = std::getenv(key);
  return v ? std::string(v) : std::string(defv);
}

inline std::string trim(const std::string& s) {
  auto start = s.find_first_not_of(" \t\r\n");
  auto end = s.find_last_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  return s.substr(start, end - start + 1);
}

inline void load_from_ini(const std::string& path, AppConfig& c) {
  std::ifstream in(path);
  if (!in.good()) return;
  std::string line;
  std::string current_section;
  while (std::getline(in, line)) {
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    // Section header
    if (line.front() == '[' && line.back() == ']') {
      current_section = trim(line.substr(1, line.size() - 2));
      // ensure section exists
      bool found = false;
      for (auto& s : c.sections) {
        if (s.name == current_section) { found = true; break; }
      }
      if (!found) c.sections.push_back(ConfigSection{current_section, {}});
      continue;
    }
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = trim(line.substr(0, eq));
    std::string val = trim(line.substr(eq + 1));
    
    // If inside a section, store raw key/val for exchange-specific parsing
    if (!current_section.empty()) {
      for (auto& s : c.sections) {
        if (s.name == current_section) {
          s.entries.emplace_back(key, val);
          break;
        }
      }
      continue;
    }

    // Basic configuration
    if (key == "ZMQ_SUBSCRIBER_ENDPOINT") c.zmq_endpoint = val;
    else if (key == "ZMQ_TOPIC") c.zmq_topic = val;
    else if (key == "EXCHANGES") c.exchanges_csv = val;
    else if (key == "SYMBOL") c.symbol = val;
    else if (key == "MM_EXCH") c.mm_exch = val;
    else if (key == "MIN_ORDER_QTY") { 
        try { 
            c.min_order_qty = std::stod(val); 
        } catch (const std::exception& e) {
            std::cerr << "WARNING: Failed to parse MIN_ORDER_QTY: '" << val << "' - using default: " << c.min_order_qty << ". Error: " << e.what() << std::endl;
        }
    }
    else if (key == "MAX_ORDER_QTY") { 
        try { 
            c.max_order_qty = std::stod(val); 
        } catch (const std::exception& e) {
            std::cerr << "WARNING: Failed to parse MAX_ORDER_QTY: '" << val << "' - using default: " << c.max_order_qty << ". Error: " << e.what() << std::endl;
        }
    }
    else if (key == "POLL_SLEEP_MS") { 
        try { 
            c.poll_sleep_ms = std::stoi(val); 
        } catch (const std::exception& e) {
            std::cerr << "WARNING: Failed to parse POLL_SLEEP_MS: '" << val << "' - using default: " << c.poll_sleep_ms << ". Error: " << e.what() << std::endl;
        }
    }
    
    // Message handler configuration (format: HANDLER_<name>_<property>)
    else if (key.rfind("HANDLER_", 0) == 0) {
      auto last_us = key.rfind('_');
      if (last_us != std::string::npos && last_us > 8) {
        std::string handler_name = key.substr(8, last_us - 8);
        std::string property = key.substr(last_us + 1);

        // Find or create handler config
        MessageHandlerConfig* handler_config = nullptr;
        for (auto& h : c.message_handlers) {
          if (h.name == handler_name) {
            handler_config = &h;
            break;
          }
        }
        if (!handler_config) {
          c.message_handlers.push_back(MessageHandlerConfig{handler_name, "", "", true});
          handler_config = &c.message_handlers.back();
        }

        // Set property
        if (property == "ENDPOINT") handler_config->endpoint = val;
        else if (property == "TOPIC") handler_config->topic = val;
        else if (property == "ENABLED") handler_config->enabled = (val == "true" || val == "1");
      }
    }
    
    // External MD/ORD/POS buses
    else if (key == "MD_PUB_ENDPOINT") c.md_pub_endpoint = val;
    else if (key == "MD_SUB_ENDPOINT") c.md_sub_endpoint = val;
    else if (key == "MD_TOPIC") c.md_topic = val;
    else if (key == "ORD_PUB_ENDPOINT") c.ord_pub_endpoint = val;
    else if (key == "ORD_SUB_ENDPOINT") c.ord_sub_endpoint = val;
    else if (key == "ORD_TOPIC_NEW") c.ord_topic_new = val;
    else if (key == "ORD_TOPIC_EV") c.ord_topic_ev = val;
    else if (key == "POS_PUB_ENDPOINT") c.pos_pub_endpoint = val;
    else if (key == "POS_SUB_ENDPOINT") c.pos_sub_endpoint = val;
    else if (key == "POS_TOPIC") c.pos_topic = val;

    // Quote server specific
    else if (key == "WEBSOCKET_URL") c.websocket_url = val;
    else if (key == "PUBLISH_RATE_HZ") { 
        try { 
            c.publish_rate_hz = std::stod(val); 
        } catch (const std::exception& e) {
            std::cerr << "WARNING: Failed to parse PUBLISH_RATE_HZ: '" << val << "' - using default: " << c.publish_rate_hz << ". Error: " << e.what() << std::endl;
        }
    }
    else if (key == "MAX_DEPTH") { 
        try { 
            c.max_depth = std::stoi(val); 
        } catch (const std::exception& e) {
            std::cerr << "WARNING: Failed to parse MAX_DEPTH: '" << val << "' - using default: " << c.max_depth << ". Error: " << e.what() << std::endl;
        }
    }
    else if (key == "PARSER") c.parser = val;
    else if (key == "SNAPSHOT_ONLY") c.snapshot_only = (val == "1" || val == "true" || val == "TRUE");
    else if (key == "BOOK_DEPTH") { 
        try { 
            c.book_depth = std::stoi(val); 
        } catch (const std::exception& e) {
            std::cerr << "WARNING: Failed to parse BOOK_DEPTH: '" << val << "' - using default: " << c.book_depth << ". Error: " << e.what() << std::endl;
        }
    }
  }
}

inline AppConfig load_app_config() {
  AppConfig c;
  // defaults
  c.zmq_endpoint = "tcp://127.0.0.1:5555";
  c.zmq_topic = "inventory_update";
  c.exchanges_csv = "GRVT";
  c.symbol = "ETHUSDC-PERP";
  c.mm_exch = "";
  c.min_order_qty = 0.0;
  c.max_order_qty = 0.0;
  c.poll_sleep_ms = 0;
  c.md_pub_endpoint = "tcp://127.0.0.1:6001";
  c.md_sub_endpoint = "tcp://127.0.0.1:6001";
  c.md_topic = "";
  c.ord_pub_endpoint = "tcp://127.0.0.1:6002"; // orders out
  c.ord_sub_endpoint = "tcp://127.0.0.1:6003"; // events in
  c.ord_topic_new = "ord.new";
  c.ord_topic_ev = "ord.ev";
  c.pos_pub_endpoint = "tcp://127.0.0.1:6004"; // positions out
  c.pos_sub_endpoint = "tcp://127.0.0.1:6004"; // positions in
  c.pos_topic = "";

  // load from config.ini if present
  auto ini_path = getenv_or("CPP_CONFIG", "./cpp/config.ini");
  load_from_ini(ini_path, c);
  return c;
}


