#pragma once

#include <stdint.h>

// Deribit WebSocket client using libwebsockets
struct DeribitLwsCallbacks {
  void (*on_open)(void* user);
  void (*on_message)(void* user, const char* data, int len);
  void (*on_close)(void* user);
};

#ifdef __cplusplus
extern "C" {
#endif

// Start Deribit WebSocket client
// Returns 0 on success, -1 on error
int deribit_lws_run(const char* host, int port, int use_ssl, 
                   const char* path, volatile int* runflag, 
                   void* user_data, struct DeribitLwsCallbacks callbacks);

// Stop the client (sets runflag to 0)
void deribit_lws_stop(volatile int* runflag);

#ifdef __cplusplus
}
#endif
