#include "websocket_transport.hpp"
#include "libuv_websocket_transport.hpp"
#include <iostream>

namespace websocket_transport {

std::unique_ptr<IWebSocketTransport> WebSocketTransportFactory::create() {
    return std::make_unique<LibuvWebSocketTransport>();
}

} // namespace websocket_transport