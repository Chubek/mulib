/* Complete, offline microjs-promise example. */
#define MULIB_IMPL_MICROJS_PROMISE
#include "mulib-microjs-promise.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static void
increment (void *ctx)
{
  (*(unsigned *)ctx)++;
}

int
main (void)
{
  mulib_microjs_microtasks_t q = { 0 };
  unsigned count = 0;
  assert (mulib_microjs_microtask_post (&q, increment, &count));
  assert (mulib_microjs_microtask_drain (&q, 10) == 1 && count == 1);
  mulib_microjs_microtask_free (&q);
  return 0;
}
