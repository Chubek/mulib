#define MULIB_IMPL_WEBSTORAGE
#include "mulib-webstorage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
main (void)
{
  mulib_webstorage_t s = { 0 };
  /* One entry, four string bytes including both terminating NULs. */
  assert (mulib_webstorage_set_limited (&s, "x", "y", 1, 4));
  assert (!strcmp (mulib_webstorage_get (&s, "x"), "y"));
  assert (!mulib_webstorage_set_limited (&s, "z", "more", 1, 4));
  assert (mulib_webstorage_remove (&s, mulib_webstorage_key (&s, 0)));
  assert (mulib_webstorage_key (&s, 0) == NULL);
  mulib_webstorage_free (&s);
  return 0;
}
