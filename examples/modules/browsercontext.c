#define MULIB_IMPL_BROWSERCONTEXT
#include "mulib-browsercontext.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  mulib_browsercontext_t c;
  assert (mulib_browsercontext_init (&c, 1, 1));
  assert (c.private_mode == 1);
  return 0;
}
