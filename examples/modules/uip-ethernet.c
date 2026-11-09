#define MULIB_IMPL_UIP_ETHERNET
#include "mulib-uip-ethernet.h"
#include <assert.h>
int
main (void)
{
  uint8_t dst[6] = { 2 }, src[6] = { 4 }, frame[32];
  size_t n = 0;
  assert (!mulib_uip_ethernet_pack (frame, sizeof frame, dst, src, 0x0800,
                                    "data", 4, &n));
  mulib_uip_ethernet_frame_t view;
  assert (!mulib_uip_ethernet_parse (frame, n, &view));
  assert (view.length == 4);
  return 0;
}
