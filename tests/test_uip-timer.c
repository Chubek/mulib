/* Generated, standalone MuLIB add-on symbol and API test: uip-timer */
#include <assert.h>
#include <errno.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define MULIB_IMPL_UIP_TIMER
#include "mulib-uip-timer.h"
int
main (void)
{
  {
    void (*volatile p) (void) = (void (*) (void))&mulib_uip_timer_monotonic_ms;
    assert (p);
  }

  uint64_t ms = 0;
  assert (!mulib_uip_timer_monotonic_ms (&ms));
  assert (ms > 0);
  puts ("PASS uip-timer");
  return 0;
}
