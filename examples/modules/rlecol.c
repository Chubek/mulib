#define MULIB_IMPL_RLECOL
#include "mulib-rlecol.h"
#include <assert.h>
int
main (void)
{
  uint64_t values[] = { 7, 7, 7, 9 }, decoded[4];
  mulib_rlecol_run_t runs[2];
  assert (mulib_rlecol_encode (values, 4, runs, 2) == 2 && runs[0].count == 3);
  assert (mulib_rlecol_decode (runs, 2, decoded, 4) == 4
          && !memcmp (values, decoded, sizeof (values)));
  return 0;
}
