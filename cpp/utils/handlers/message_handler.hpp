#pragma once
#include <string>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>

// Forward declaration
class ZmqSubscriber;

// Base interface for message handlers
class IMessageHandler {
public:
  using DataCallback = std::function<void(const std::string& handler_name, const std::string& data)>;
  
  virtual ~IMessageHandler() = default;
  
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual bool is_running() const = 0;
  
  virtual std::string get_name() const = 0;
  virtual std::string get_topic() const = 0;
  virtual std::string get_endpoint() const = 0;
  
  virtual void set_data_callback(DataCallback callback) = 0;
};

// Concrete message handler implementation
class MessageHandler : public IMessageHandler {
public:
  MessageHandler(const std::string& name, 
                const std::string& endpoint, 
                const std::string& topic);
  ~MessageHandler();
  
  void start() override;
  void stop() override;
  bool is_running() const override { return running_.load(); }
  
  std::string get_name() const override { return name_; }
  std::string get_topic() const override { return topic_; }
  std::string get_endpoint() const override { return endpoint_; }
  
  void set_data_callback(DataCallback callback) override { data_callback_ = callback; }

private:
  void process_messages();
  
  std::string name_;
  std::string endpoint_;
  std::string topic_;
  std::atomic<bool> running_{false};
  std::unique_ptr<ZmqSubscriber> subscriber_;
  std::unique_ptr<std::thread> handler_thread_;
  DataCallback data_callback_;
};
