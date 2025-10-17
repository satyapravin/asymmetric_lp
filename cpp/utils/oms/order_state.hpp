#pragma once
#include <string>
#include <cstdint>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include "types.hpp"

// Enhanced order state management
enum class OrderState {
  PENDING,      // Order submitted, waiting for exchange response
  ACKNOWLEDGED, // Exchange acknowledged the order
  PARTIALLY_FILLED, // Order partially filled
  FILLED,       // Order completely filled
  CANCELLED,    // Order cancelled
  REJECTED,     // Order rejected by exchange
  EXPIRED       // Order expired (time-based)
};

inline const char* to_string(OrderState state) {
  switch (state) {
    case OrderState::PENDING: return "PENDING";
    case OrderState::ACKNOWLEDGED: return "ACKNOWLEDGED";
    case OrderState::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
    case OrderState::FILLED: return "FILLED";
    case OrderState::CANCELLED: return "CANCELLED";
    case OrderState::REJECTED: return "REJECTED";
    case OrderState::EXPIRED: return "EXPIRED";
  }
  return "UNKNOWN";
}

// Enhanced order with state tracking
struct OrderStateInfo {
  std::string cl_ord_id;
  std::string exch;
  std::string symbol;
  Side side{Side::Buy};
  double qty{0.0};
  double price{0.0};
  bool is_market{true};
  
  // State management
  OrderState state{OrderState::PENDING};
  double filled_qty{0.0};
  double avg_fill_price{0.0};
  std::string exchange_order_id; // Exchange's internal order ID
  
  // Timestamps
  std::chrono::system_clock::time_point created_time;
  std::chrono::system_clock::time_point last_update_time;
  
  // Additional metadata
  std::string reject_reason;
  uint32_t retry_count{0};
  bool is_retryable{true};
  
  OrderStateInfo() {
    created_time = std::chrono::system_clock::now();
    last_update_time = created_time;
  }
};

// Order state machine transitions
class OrderStateMachine {
public:
  static bool isValidTransition(OrderState from, OrderState to) {
    switch (from) {
      case OrderState::PENDING:
        return to == OrderState::ACKNOWLEDGED || 
               to == OrderState::REJECTED || 
               to == OrderState::EXPIRED;
               
      case OrderState::ACKNOWLEDGED:
        return to == OrderState::PARTIALLY_FILLED || 
               to == OrderState::FILLED || 
               to == OrderState::CANCELLED || 
               to == OrderState::REJECTED || 
               to == OrderState::EXPIRED;
               
      case OrderState::PARTIALLY_FILLED:
        return to == OrderState::FILLED || 
               to == OrderState::CANCELLED || 
               to == OrderState::REJECTED || 
               to == OrderState::EXPIRED;
               
      case OrderState::FILLED:
      case OrderState::CANCELLED:
      case OrderState::REJECTED:
      case OrderState::EXPIRED:
        return false; // Terminal states
    }
    return false;
  }
  
  static OrderState getNextState(OrderState current, OrderEventType event_type) {
    switch (current) {
      case OrderState::PENDING:
        switch (event_type) {
          case OrderEventType::Ack: return OrderState::ACKNOWLEDGED;
          case OrderEventType::Reject: return OrderState::REJECTED;
          default: return current;
        }
        
      case OrderState::ACKNOWLEDGED:
        switch (event_type) {
          case OrderEventType::Fill: return OrderState::PARTIALLY_FILLED;
          case OrderEventType::Cancel: return OrderState::CANCELLED;
          case OrderEventType::Reject: return OrderState::REJECTED;
          default: return current;
        }
        
      case OrderState::PARTIALLY_FILLED:
        switch (event_type) {
          case OrderEventType::Fill: return OrderState::FILLED;
          case OrderEventType::Cancel: return OrderState::CANCELLED;
          case OrderEventType::Reject: return OrderState::REJECTED;
          default: return current;
        }
        
      default:
        return current;
    }
  }
};
