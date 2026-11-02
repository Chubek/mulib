#define MULIB_IMPL_HTTP_RESPONSE
#define MULIB_IMPL_WEBFRAMEWORK
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_BUFFER
#include "mulib-http_response.h"
#include <assert.h>
int
main (void)
{
  mulib_http_message_t response = { 0 };
  assert (mulib_http_response_create (&response, 200, "text/plain", "hello", 5)
          == MULIB_HTTP_OK);
  assert (response.status == 200 && response.body_length == 5);
  mulib_http_message_free (&response);
  return 0;
}
