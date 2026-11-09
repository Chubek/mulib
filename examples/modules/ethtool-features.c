#define MULIB_IMPL_ETHTOOL_FEATURES
#include "mulib-ethtool-features.h"
#include <stdio.h>
int
main (void)
{
  mulib_ethtool_features_block_t b = { 0 };
  int r = mulib_ethtool_features_get ("lo", 0, &b);
  printf ("features: status=%d active=%x\n", r, b.active);
  return 0;
}
