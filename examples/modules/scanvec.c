#define MULIB_IMPL_SCANVEC
#include "mulib-scanvec.h"
#include <assert.h>
int
main (void)
{
  double values[] = { 5, 10, NAN, 20 };
  size_t rows[4];
  assert (mulib_scanvec_double (values, 4, MULIB_SCAN_GE, 10, rows, 4) == 2);
  assert (rows[0] == 1 && rows[1] == 3);
  return 0;
}
