#define MULIB_IMPL_WEB_CONNECTION
#define MULIB_IMPL_HTTP1
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_BUFFER
#include "mulib-web_conntection.h"
#include <assert.h>
int
main (void)
{
  const char wire[] = "GET / HTTP/1.1\r\nHost: example.org\r\n\r\n";
  mulib_web_connection_t connection = { 0 };
  mulib_http_message_t request = { 0 };
  connection.limit = 1000;
  assert (mulib_web_connection_feed (&connection, wire, 10, 0)
          == MULIB_HTTP_OK);
  assert (mulib_web_connection_next (&connection, &request) == MULIB_HTTP_MORE);
  assert (
      mulib_web_connection_feed (&connection, wire + 10, sizeof (wire) - 11, 1)
      == MULIB_HTTP_OK);
  assert (mulib_web_connection_next (&connection, &request) == MULIB_HTTP_OK);
  mulib_http_message_free (&request);
  mulib_web_connection_free (&connection);
  return 0;
}
