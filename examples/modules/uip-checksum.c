#define MULIB_IMPL_UIP_CHECKSUM
#include "mulib-uip-checksum.h"
#include <assert.h>
int
main (void)
{
  const uint8_t words[] = { 0xff, 0xff, 0xff, 0xff };
  assert (!mulib_uip_checksum (words, sizeof words));
  return 0;
}
