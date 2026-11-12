/* Complete, offline webidl-runtime example. */
#define MULIB_IMPL_SHYLOCK_CORE
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_WEBIDL_RUNTIME
#include "mulib-webidl-runtime.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  int32_t value;
  assert (mulib_webidl_to_int32 (2.5, 1, &value) && value == 2);
  assert (!mulib_webidl_to_int32 (2.5, 0, &value));
  mulib_shylock_buffer_t text = { 0 };
  assert (mulib_webidl_domstring_utf8 ("hello", 5, &text));
  mulib_shylock_buffer_free (&text);
  return 0;
}
