#define MULIB_IMPL_HTTPSERVER_CONFIG
#define MULIB_IMPL_HTTP
#define MULIB_IMPL_URI
#define MULIB_IMPL_INET4
#define MULIB_IMPL_INET6
#define MULIB_IMPL_INTE6
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_BUFFER
#include "mulib-httpserver_config.h"
#include <assert.h>
int
main (void)
{
  mulib_httpserver_config_t config = mulib_httpserver_config_default ();
  assert (mulib_httpserver_config_validate (&config));
  config.max_buffer_bytes = 1;
  assert (!mulib_httpserver_config_validate (&config));
  return 0;
}
