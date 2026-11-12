/* Complete, offline browser-cookiejar example. */
#define MULIB_IMPL_BROWSER_COOKIEJAR
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-browser-cookiejar.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_browser_cookiejar_t jar = { 0 };
  mulib_browser_cookie_t cookie
      = { "sid", "abc", "example.com", "/", 0, 1, 1, 1 };
  assert (mulib_browser_cookiejar_put (&jar, &cookie));
  mulib_shylock_buffer_t header = { 0 };
  assert (mulib_browser_cookiejar_header (&jar, "example.com", "/", 1, 100,
                                          &header));
  assert (!strcmp (header.data, "sid=abc"));
  mulib_shylock_buffer_free (&header);
  mulib_browser_cookiejar_free (&jar);
  return 0;
}
