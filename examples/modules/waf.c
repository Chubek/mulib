#define MULIB_IMPL_WAF
#define MULIB_IMPL_SECURITY
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_BUFFER
#include "mulib-waf.h"
#include <assert.h>
int
main (void)
{
  const char wire[] = "GET / HTTP/1.1\r\nHost: example.org\r\n\r\n";
  const char *methods[] = { "GET" };
  mulib_waf_t policy = { 1000, 1000, methods, 1 };
  mulib_http_message_t request = { 0 };
  size_t used;
  assert (mulib_http_parse (wire, sizeof (wire) - 1, 0, NULL, &request, &used)
          == MULIB_HTTP_OK);
  assert (mulib_waf_check (&policy, &request));
  mulib_http_message_free (&request);
  return 0;
}
