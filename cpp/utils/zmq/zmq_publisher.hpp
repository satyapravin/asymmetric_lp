#pragma once
#include <string>
#include <memory>
#include <atomic>
#include <zmq.h>

/**
 * ZeroMQ Publisher for high-performance message publishing
 * 
 * Thread-safe publisher that supports:
 * - Topic-based message filtering
 * - Non-blocking sends (drops messages if buffer full)
 * - Message conflation (keeps only latest message per topic)
 * - High water mark configuration
 * 
 * @note Default behavior is non-blocking to prevent publisher stalling.
 *       Messages are dropped if send buffer is full (ZMQ_DONTWAIT).
 */
class ZmqPublisher {
public:
  /**
   * Construct a ZMQ publisher
   * 
   * @param bind_endpoint ZMQ endpoint to bind to (e.g., "tcp://127.0.0.1:5555")
   * @param hwm High water mark (maximum queued messages)
   * @param conflate If true, keep only latest message per topic (for state updates)
   * 
   * @throws std::runtime_error if ZMQ context or socket creation fails
   */
  ZmqPublisher(const std::string& bind_endpoint, int hwm = 1000, bool conflate = false);
  
  /**
   * Destructor - properly cleans up ZMQ resources
   */
  ~ZmqPublisher();

  // Non-copyable
  ZmqPublisher(const ZmqPublisher&) = delete;
  ZmqPublisher& operator=(const ZmqPublisher&) = delete;

  /**
   * Bind to the configured endpoint
   * 
   * @return true if bind succeeded, false otherwise
   * 
   * @note Called automatically in constructor, but can be called again if needed.
   */
  bool bind();
  
  /**
   * Send a message with binary data
   * 
   * @param topic Message topic (for subscriber filtering)
   * @param data Pointer to data to send
   * @param size Size of data in bytes
   * @param flags ZMQ send flags (default: ZMQ_DONTWAIT for non-blocking)
   * @return true if message was sent, false if dropped or error occurred
   * 
   * @note Messages are dropped (not queued) if send buffer is full.
   * @note This method is thread-safe.
   */
  bool send(const std::string& topic, const void* data, size_t size, int flags = ZMQ_DONTWAIT);
  
  /**
   * Send a string message
   * 
   * @param topic Message topic (for subscriber filtering)
   * @param payload String payload to send
   * @param flags ZMQ send flags (default: ZMQ_DONTWAIT for non-blocking)
   * @return true if message was sent, false if dropped or error occurred
   * 
   * @note Convenience wrapper around send() for string data.
   * @note This method is thread-safe.
   */
  bool send_string(const std::string& topic, const std::string& payload, int flags = ZMQ_DONTWAIT);
  
  /**
   * Publish a message (backward-compatible API)
   * 
   * @param topic Message topic
   * @param payload String payload
   * @return true if message was sent, false otherwise
   * 
   * @note Uses non-blocking send by default.
   */
  bool publish(const std::string& topic, const std::string& payload) { 
    return send_string(topic, payload, ZMQ_DONTWAIT); 
  }
  
  // Statistics
  uint64_t get_messages_sent() const { return messages_sent_.load(); }
  uint64_t get_messages_dropped() const { return messages_dropped_.load(); }
  
  // Check if send buffer is approaching full (for monitoring)
  bool is_buffer_available() const;

private:
  void* ctx_;
  void* pub_;
  std::string endpoint_;
  int hwm_;
  bool bound_;
  bool conflate_;
  
  // Statistics tracking
  std::atomic<uint64_t> messages_sent_{0};
  std::atomic<uint64_t> messages_dropped_{0};
};
