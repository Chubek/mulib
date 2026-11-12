/* Complete, offline shylock-core example. */
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#include "mulib-shylock-core.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_shylock_buffer_t b = { 0 };
  b.limit = 128;
  assert (mulib_shylock_text (&b, "hello"));
  assert (mulib_shylock_append (&b, b.data, b.len));
  assert (!strcmp (b.data, "hellohello"));
  mulib_shylock_buffer_free (&b);
  return 0;
}
