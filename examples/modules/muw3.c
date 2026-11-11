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
#include "detail/mulib-tls-implementation.h"
#include "mulib-muw3.h"
#include <assert.h>
#include <stdio.h>
static int fixture (void *user, const mulib_uri_t *endpoint, const void *request,
    size_t length, mulib_webclient_receive_fn receive, void *receiver)
{
  static const char reply[] = "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nMuW3!";
  (void)user; (void)endpoint; (void)request; (void)length;
  return receive (receiver, reply, sizeof reply - 1);
}
int main (void)
{
  mulib_muw3_client_t client = mulib_muw3_client_default ();
  mulib_muw3_request_t request = {0};
  mulib_muw3_response_t response = {0};
  client.http = fixture; /* Leave NULL to use real POSIX HTTP/DNS. */
  request.url = "http://example.test/";
  assert (mulib_muw3_perform (&client, &request, &response) == MULIB_MUW3_OK);
  assert (fwrite (response.message.body, 1, response.message.body_length, stdout) == 5);
  putchar ('\n'); mulib_muw3_response_free (&response);
  return 0;
}
