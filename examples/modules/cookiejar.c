#define MULIB_IMPL_COOKIEJAR
#include "mulib-cookiejar.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  mulib_cookiejar_t j = { 0 };
  assert (mulib_cookiejar_set (&j, "a", "1"));
  assert (mulib_cookiejar_set (&j, "a", "2"));
  assert (!strcmp (mulib_cookiejar_get (&j, "a"), "2"));
  mulib_cookiejar_free (&j);
  return 0;
}
