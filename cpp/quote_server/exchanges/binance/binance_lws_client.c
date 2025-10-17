#include "binance_lws_client.h"
#include <libwebsockets.h>
#include <string.h>

struct SessionData {
  void* user;
  struct BinanceLwsCallbacks cbs;
};

static int lws_cb(struct lws* wsi, enum lws_callback_reasons reason, void* user, void* in, size_t len) {
  struct SessionData* s = (struct SessionData*)user;
  switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      if (s && s->cbs.on_open) s->cbs.on_open(s->user);
      break;
    case LWS_CALLBACK_CLIENT_RECEIVE:
      if (s && s->cbs.on_message && in && len > 0) s->cbs.on_message(s->user, (const char*)in, (int)len);
      break;
    case LWS_CALLBACK_CLIENT_CLOSED:
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      if (s && s->cbs.on_close) s->cbs.on_close(s->user);
      break;
    default:
      break;
  }
  return 0;
}

int binance_lws_run(const char* host,
                    int port,
                    int use_ssl,
                    const char* path,
                    volatile int* running_flag,
                    void* user,
                    struct BinanceLwsCallbacks cbs) {
  struct lws_context_creation_info info;
  memset(&info, 0, sizeof info);
  struct lws_protocols protocols[] = {
    {"binance", lws_cb, sizeof(struct SessionData), 0, 0, NULL, 0},
    {NULL, NULL, 0, 0, 0, NULL, 0}
  };
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  struct lws_context* context = lws_create_context(&info);
  if (!context) return -1;

  struct SessionData psd;
  psd.user = user;
  psd.cbs = cbs;

  struct lws_client_connect_info ccinfo;
  memset(&ccinfo, 0, sizeof ccinfo);
  ccinfo.context = context;
  ccinfo.address = host;
  ccinfo.port = port;
  ccinfo.path = path;
  ccinfo.host = host;
  ccinfo.origin = host;
  ccinfo.ssl_connection = use_ssl ? LCCSCF_USE_SSL : 0;
  ccinfo.protocol = protocols[0].name;
  ccinfo.userdata = &psd;

  struct lws* wsi = lws_client_connect_via_info(&ccinfo);
  if (!wsi) {
    lws_context_destroy(context);
    return -2;
  }

  while (*running_flag) {
    lws_service(context, 50);
  }
  lws_context_destroy(context);
  return 0;
}


