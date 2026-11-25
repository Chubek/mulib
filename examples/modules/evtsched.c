#define MULIB_IMPL_EVTSCHED
#include "mulib-evtsched.h"
#include <stdio.h>

/* Schedule the next occurrence from the observed clock. */
static void
tick (mulib_evtsched_notice_t *notice, int64_t now, void *context)
{
  unsigned *remaining = (unsigned *)notice->data;
  puts ("tick");
  if (--*remaining)
    mulib_evtsched_schedule ((mulib_evtsched_t *)context, notice, now + 60);
}

int
main (void)
{
  mulib_evtsched_t scheduler = { 0 };
  mulib_evtsched_notice_t alarm = { 0 };
  unsigned remaining = 2;
  alarm.data = &remaining;
  if (mulib_evtsched_schedule (&scheduler, &alarm, 10) < 0)
    return 1;
  for (int64_t now = 10; now <= 70; ++now)
    mulib_evtsched_dispatch (&scheduler, now, tick, &scheduler);
  mulib_evtsched_clear (&scheduler);
  return remaining != 0;
}
