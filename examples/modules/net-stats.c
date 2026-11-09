#define MULIB_IMPL_NET_STATS
#include "mulib-net-stats.h"
#include <inttypes.h>
int
main (void)
{
  mulib_net_stats_t s;
  int r = mulib_net_stats_read ("lo", &s);
  if (!r)
    printf ("loopback RX=%" PRIu64 " TX=%" PRIu64 " bytes\n", s.rx_bytes,
            s.tx_bytes);
  return !!r;
}
