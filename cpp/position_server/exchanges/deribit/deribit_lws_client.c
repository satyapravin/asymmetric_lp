#include "deribit_lws_client.h"
#include <libwebsockets.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct deribit_context {
  struct lws_context* context;
  struct lws* wsi;
  volatile int* runflag;
  void* user_data;
  struct DeribitLwsCallbacks callbacks;
  char host[256];
  char path[512];
  int port;
  int use_ssl;
};

static int deribit_lws_callback(struct lws* wsi, enum lws_callback_reasons reason,
                               void* user, void* in, size_t len) {
  struct deribit_context* ctx = (struct deribit_context*)user;
  
  switch (reason) {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
      printf("[DERIBIT_LWS] Connected to %s:%d%s\n", ctx->host, ctx->port, ctx->path);
      if (ctx->callbacks.on_open) {
        ctx->callbacks.on_open(ctx->user_data);
      }
      break;
      
    case LWS_CALLBACK_CLIENT_RECEIVE:
      if (ctx->callbacks.on_message) {
        ctx->callbacks.on_message(ctx->user_data, (const char*)in, (int)len);
      }
      break;
      
    case LWS_CALLBACK_CLIENT_CLOSED:
      printf("[DERIBIT_LWS] Connection closed\n");
      if (ctx->callbacks.on_close) {
        ctx->callbacks.on_close(ctx->user_data);
      }
      break;
      
    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
      printf("[DERIBIT_LWS] Connection error\n");
      if (ctx->callbacks.on_close) {
        ctx->callbacks.on_close(ctx->user_data);
      }
      break;
      
    default:
      break;
  }
  
  return 0;
}

static struct lws_protocols protocols[] = {
  {
    "deribit-protocol",
    deribit_lws_callback,
    0,
    4096,
  },
  { NULL, NULL, 0, 0 }
};

int deribit_lws_run(const char* host, int port, int use_ssl, 
                   const char* path, volatile int* runflag, 
                   void* user_data, struct DeribitLwsCallbacks callbacks) {
  struct lws_context_creation_info info;
  struct deribit_context ctx;
  struct lws_client_connect_info ccinfo;
  
  memset(&info, 0, sizeof(info));
  memset(&ctx, 0, sizeof(ctx));
  memset(&ccinfo, 0, sizeof(ccinfo));
  
  // Store parameters
  strncpy(ctx.host, host, sizeof(ctx.host) - 1);
  strncpy(ctx.path, path, sizeof(ctx.path) - 1);
  ctx.port = port;
  ctx.use_ssl = use_ssl;
  ctx.runflag = runflag;
  ctx.user_data = user_data;
  ctx.callbacks = callbacks;
  
  // Create libwebsockets context
  info.port = CONTEXT_PORT_NO_LISTEN;
  info.protocols = protocols;
  info.gid = -1;
  info.uid = -1;
  info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
  
  ctx.context = lws_create_context(&info);
  if (!ctx.context) {
    printf("[DERIBIT_LWS] Failed to create context\n");
    return -1;
  }
  
  // Set up connection info
  ccinfo.context = ctx.context;
  ccinfo.address = ctx.host;
  ccinfo.port = ctx.port;
  ccinfo.path = ctx.path;
  ccinfo.host = ctx.host;
  ccinfo.origin = ctx.host;
  ccinfo.protocol = protocols[0].name;
  ccinfo.userdata = &ctx;
  
  if (ctx.use_ssl) {
    ccinfo.ssl_connection = LCCSCF_USE_SSL;
  }
  
  // Connect
  printf("[DERIBIT_LWS] Attempting to connect to %s:%d%s (SSL: %s)\n", host, port, path, ctx.use_ssl ? "yes" : "no");
  ctx.wsi = lws_client_connect_via_info(&ccinfo);
  if (!ctx.wsi) {
    printf("[DERIBIT_LWS] Failed to connect to %s:%d%s\n", host, port, path);
    lws_context_destroy(ctx.context);
    return -1;
  }
  
  printf("[DERIBIT_LWS] Starting event loop for %s:%d%s\n", host, port, path);
  
  // Event loop
  int ret;
  while (*runflag && (ret = lws_service(ctx.context, 50)) >= 0) {
    // Continue processing
  }
  
  if (ret < 0) {
    printf("[DERIBIT_LWS] lws_service returned error: %d\n", ret);
  }
  
  printf("[DERIBIT_LWS] Event loop ended\n");
  
  // Cleanup
  lws_context_destroy(ctx.context);
  
  return 0;
}

void deribit_lws_stop(volatile int* runflag) {
  if (runflag) {
    *runflag = 0;
  }
}
