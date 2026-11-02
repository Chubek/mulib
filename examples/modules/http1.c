#define MULIB_IMPL_HTTP1
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_BUFFER
#include "mulib-http1.h"
#include <assert.h>
int
main (void)
{
  const char wire[] = "GET / HTTP/1.1\r\nHost: example.org\r\n\r\n";
  mulib_http_message_t request = { 0 };
  size_t used;
  assert (mulib_http1_parse (wire, sizeof (wire) - 1, 0, NULL, &request, &used)
          == MULIB_HTTP_OK);
  assert (used == sizeof (wire) - 1);
  mulib_http_message_free (&request);
  return 0;
}
