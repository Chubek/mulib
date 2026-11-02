#define MULIB_IMPL_HTTP_REQUEST
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_BUFFER
#include "mulib-http_request.h"
#include <assert.h>
int
main (void)
{
  mulib_http_message_t request = { 0 };
  mulib_http_header_t host = { (char *)"Host", (char *)"example.org" };
  assert (mulib_http_request_create (&request, "GET", "/", &host, 1, NULL, 0)
          == MULIB_HTTP_OK);
  assert (!strcmp (request.target, "/"));
  mulib_http_message_free (&request);
  return 0;
}
