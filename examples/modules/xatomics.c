#define MULIB_IMPL_XATOMICS
#include "mulib-xatomics.h"
#include <assert.h>
int main (void)
{
  mulib_xatomic_u32_t counter = MULIB_XATOMIC_INIT (0);
  uint32_t expected = 1;
  assert (mulib_xatomic_u32_fetch_add (&counter, 1) == 0);
  assert (mulib_xatomic_u32_compare_exchange (&counter, &expected, 42));
  assert (mulib_xatomic_u32_load (&counter) == 42); return 0;
}
