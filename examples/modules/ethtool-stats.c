#define MULIB_IMPL_ETHTOOL_STATS
#include "mulib-ethtool-stats.h"
#include <stdio.h>
int
main (void)
{
  char text[64];
  int r = mulib_ethtool_stats_sysfs ("lo", text, sizeof text);
  if (r >= 0)
    fputs (text, stdout);
  else
    printf ("stats status=%d\n", r);
  return 0;
}
