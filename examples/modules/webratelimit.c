#define MULIB_IMPL_WEBRATELIMIT
#include "mulib-webratelimit.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  mulib_webratelimit_t r;
  mulib_webratelimit_init (&r, 100);
  assert (mulib_webratelimit_take (&r, 1000));
  assert (!mulib_webratelimit_take (&r, 1001));
  assert (mulib_webratelimit_take (&r, 1100));
  return 0;
}
