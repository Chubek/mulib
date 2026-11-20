/* Complete, offline bounded-text example. */
#define MULIB_IMPL_BOUNDED_TEXT
#define MULIB_IMPL_UTF8
#include "mulib-bounded-text.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_bounded_text_buffer_t b = { 0 };
  b.limit = 128;
  assert (mulib_bounded_text_text (&b, "hello"));
  assert (mulib_bounded_text_append (&b, b.data, b.len));
  assert (!strcmp (b.data, "hellohello"));
  mulib_bounded_text_buffer_free (&b);
  return 0;
}
