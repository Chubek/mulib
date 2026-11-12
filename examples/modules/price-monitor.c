/* Complete, offline price-monitor example. */
#define MULIB_IMPL_MONEY
#define MULIB_IMPL_PRICE_MONITOR
#define MULIB_IMPL_PRICE_OBSERVATION
#include "mulib-price-monitor.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int
main (void)
{
  mulib_price_observation_t old = { 0 };
  strcpy (old.product, "p");
  strcpy (old.merchant, "m");
  assert (mulib_money_make (&old.price, 100, "USD", 2));
  mulib_price_observation_t next = old;
  next.price.minor = 80;
  mulib_price_monitor_event_t event;
  assert (mulib_price_monitor_compare (&old, &next, .1, &event)
          && event == MULIB_PRICE_MONITOR_DOWN);
  return 0;
}
