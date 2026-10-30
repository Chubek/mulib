#define MULIB_IMPL_ABSTUN
#include "mulib-abstun.h"
#include <assert.h>
static int
send_packet (void *ctx, const void *p, size_t n)
{
  (void)p;
  *(size_t *)ctx = n;
  return 0;
}
int
main (void)
{
  size_t sent = 0;
  mulib_abstun_t tun = { &sent, 1500, NULL, send_packet };
  assert (!mulib_abstun_send (&tun, "packet", 6) && sent == 6);
  assert (mulib_abstun_send (&tun, "", 0) == -1);
  return 0;
}
