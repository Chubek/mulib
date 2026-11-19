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
#define MULIB_IMPL_CDD_GRPC
#include "detail/mulib-tls-implementation.h"
#include "mulib-grpc.h"
#include <stdio.h>

static int fixture(void *user, const mulib_uri_t *uri, const void *request,
    size_t length, mulib_webclient_receive_fn receive, void *receiver)
{
  static const char wire[] = "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/grpc-web+proto\r\nContent-Length: 26\r\n\r\n"
    "\0\0\0\0\2\010\052\200\0\0\0\16grpc-status: 0";
  (void)user;
  (void)uri;
  (void)request;
  (void)length;
  return receive(receiver, wire, sizeof(wire) - 1);
}

int main(void)
{
  static const unsigned char encoded_request[] = {0x08, 0x01};
  mulib_muw3_client_t client = mulib_muw3_client_default();
  mulib_cdd_grpc_web_reply_t reply = {0};
  int rc, ok;
  client.http = fixture; /* offline only; use a gRPC-Web endpoint for real calls */
  rc = mulib_cdd_grpc_web_unary(&client, "http://example.test/demo.Probe/Read",
      encoded_request, sizeof(encoded_request), 1024, &reply);
  if (rc != 1) return 1;
  printf("gRPC status %d; %zu response bytes\n", reply.grpc_status, reply.message_length);
  ok = reply.grpc_status == 0 && reply.message_length == 2;
  mulib_cdd_grpc_web_reply_free(&reply);
  return ok ? 0 : 1;
}
