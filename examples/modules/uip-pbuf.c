#define MULIB_IMPL_UIP_PBUF
#include "mulib-uip-pbuf.h"
#include <assert.h>
int
main (void)
{
  mulib_uip_pbuf_t p = { 0 };
  assert (!mulib_uip_pbuf_init (&p, 8, 64));
  assert (!mulib_uip_pbuf_append (&p, "payload", 7));
  assert (!mulib_uip_pbuf_prepend (&p, "hdr", 3));
  assert (!memcmp (mulib_uip_pbuf_data (&p), "hdrpayload", 10));
  mulib_uip_pbuf_destroy (&p);
  return 0;
}
