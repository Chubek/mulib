#define _POSIX_C_SOURCE 200809L
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_URI
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_WEBCLIENT
#define MULIB_IMPL_TSL
#define MULIB_IMPL_HTTPS
#define MULIB_IMPL_SOCKET
#define MULIB_IMPL_MUW3
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_JSON
#define MULIB_IMPL_CDD_REST
#define MULIB_IMPL_CDD_JSONRPC
#include "detail/mulib-tls-implementation.h"
#include "mulib-jsonrpc.h"
#include <stdio.h>

static int offline(void *user, const mulib_uri_t *uri, const void *wire, size_t n,
                   mulib_webclient_receive_fn receive, void *receiver) {
  static const char reply[] = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                             "\r\n{\"jsonrpc\":\"2.0\",\"id\":\"example-1\",\"result\":\"pong\"}";
  (void)user; (void)uri; (void)wire; (void)n;
  return receive(receiver, reply, sizeof reply - 1);
}

int main(void) {
  mulib_muw3_client_t client = mulib_muw3_client_default();
  mulib_cdd_jsonrpc_reply_t reply = {0};
  mulib_json_t *id = mulib_json_string("example-1");
  const char *text;
  int rc;
  if (!id) return 1;
  client.http = offline;
  rc = mulib_cdd_jsonrpc_call(&client, "http://example.test/rpc", "ping", NULL, id, &reply);
  mulib_json_destroy(id);
  if (rc != 1) return 1;
  text = mulib_json_text(mulib_json_get(reply.response.json, "result"), NULL);
  if (text) puts(text);
  mulib_cdd_jsonrpc_reply_free(&reply);
  return 0;
}
