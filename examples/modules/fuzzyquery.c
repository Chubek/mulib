#define MULIB_IMPL_FUZZYQUERY
#include "mulib-fuzzyquery.h"
#include <assert.h>
int
main (void)
{
  assert (mulib_fuzzyquery_distance ("kitten", 6, "sitting", 7, 3) == 3);
  assert (mulib_fuzzyquery_distance ("kitten", 6, "sitting", 7, 1) == 2);
  return 0;
}
