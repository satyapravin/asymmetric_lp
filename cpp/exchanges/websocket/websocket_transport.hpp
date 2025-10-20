#pragma once
#include "i_websocket_transport.hpp"

namespace websocket_transport {

// Transport factory
class WebSocketTransportFactory {
public:
    /**
     * Create a WebSocket transport implementation
     * @return A unique pointer to the transport implementation
     */
    static std::unique_ptr<IWebSocketTransport> create();
};

} // namespace websocket_transport