#define MULIB_IMPL_UIP_CHECKSUM
#define MULIB_IMPL_UIP_TCP
#include "mulib-uip-tcp.h"
#include <assert.h>
int
main (void)
{
  uint8_t src[4] = { 10, 0, 0, 1 }, dst[4] = { 10, 0, 0, 2 }, bytes[64];
  size_t n = 0;
  mulib_uip_tcp_segment_t segment = { 0 }, view;
  segment.source_port = 1000;
  segment.dest_port = 80;
  segment.flags = 2;
  segment.window = 1024;
  assert (!mulib_uip_tcp_pack (bytes, sizeof bytes, src, dst, &segment, &n));
  assert (!mulib_uip_tcp_parse (src, dst, bytes, n, &view));
  assert (view.flags == 2);
  return 0;
}
