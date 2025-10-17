#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*binance_on_open_cb)(void* user);
typedef void (*binance_on_message_cb)(void* user, const char* data, int len);
typedef void (*binance_on_close_cb)(void* user);

struct BinanceLwsCallbacks {
  binance_on_open_cb on_open;
  binance_on_message_cb on_message;
  binance_on_close_cb on_close;
};

int binance_lws_run(const char* host,
                    int port,
                    int use_ssl,
                    const char* path,
                    volatile int* running_flag,
                    void* user,
                    struct BinanceLwsCallbacks cbs);

#ifdef __cplusplus
}
#endif


