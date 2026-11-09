#define MULIB_IMPL_UIP_FRAGMENT
#include "mulib-uip-fragment.h"
#include <assert.h>
int
main (void)
{
  mulib_uip_frag_t fragments = { 0 };
  uint8_t src[4] = { 10, 0, 0, 1 }, dst[4] = { 10, 0, 0, 2 }, data[8] = { 1 };
  size_t complete = 0;
  assert (!mulib_uip_frag_push (&fragments, src, dst, 17, 1, 8, 0, data, 8,
                                &complete));
  assert (mulib_uip_frag_push (&fragments, src, dst, 17, 1, 0, 1, data, 8,
                               &complete)
          == 1);
  assert (complete == 16);
  return 0;
}
