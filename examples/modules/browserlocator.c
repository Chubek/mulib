#define MULIB_IMPL_BROWSERLOCATOR
#include "mulib-browserlocator.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  mulib_browserlocator_t l = { 0 };
  assert (
      mulib_browserlocator_init (&l, MULIB_BROWSERLOCATOR_CSS, "article a"));
  assert (!strcmp (l.selector, "article a"));
  mulib_browserlocator_free (&l);
  return 0;
}
