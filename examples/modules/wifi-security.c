#define MULIB_IMPL_WIFI_SECURITY
#include "mulib-wifi-security.h"
#include <assert.h>
int
main (void)
{
  const uint8_t ie[] = { 48, 18, 1,   0, 0, 15, 172, 4,  1,   0,
                         0,  15, 172, 4, 1, 0,  0,   15, 172, 2 };
  mulib_wifi_security_rsn_t rsn;
  assert (!mulib_wifi_security_rsn_parse (ie, sizeof ie, &rsn));
  assert (rsn.akm_count == 1);
  return 0;
}
