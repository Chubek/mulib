#define MULIB_IMPL_UIP_TIMER
#include "mulib-uip-timer.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
int
main (void)
{
  uint64_t now = 0;
  assert (!mulib_uip_timer_monotonic_ms (&now));
  printf ("monotonic=%" PRIu64 " ms\n", now);
  return 0;
}
