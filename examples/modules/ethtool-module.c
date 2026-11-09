#define MULIB_IMPL_ETHTOOL_MODULE
#include "mulib-ethtool-module.h"
#include <stdio.h>
int
main (void)
{
  uint32_t type = 0, bytes = 0;
  int r = mulib_ethtool_module_info ("lo", &type, &bytes);
  printf ("module: status=%d type=%u bytes=%u\n", r, type, bytes);
  return 0;
}
