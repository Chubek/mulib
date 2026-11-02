#define MULIB_IMPL_SECURITY
#include "mulib-seurity.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  char path[100];
  assert (mulib_security_path ("/hello%20world", path, sizeof (path)));
  assert (!strcmp (path, "/hello world"));
  assert (!mulib_security_path ("/%2e%2e/private", path, sizeof (path)));
  assert (mulib_security_equal ("abc", "abc", 3));
  return 0;
}
