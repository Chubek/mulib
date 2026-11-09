#define MULIB_IMPL_URLCANON
#include "mulib-urlcanon.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  char b[128];
  assert (mulib_urlcanon_normalize ("HTTPS://Example.COM:443/a#frag", b, 128));
  assert (!strcmp (b, "https://example.com/a"));
  return 0;
}
