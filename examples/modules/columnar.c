#define MULIB_IMPL_COLUMNAR
#include "mulib-columnar.h"
#include <assert.h>
int
main (void)
{
  mulib_columnar_t c = { 0 };
  double value;
  int valid;
  assert (mulib_columnar_append (&c, 3.5, 1) && mulib_columnar_append (&c, 0, 0));
  assert (mulib_columnar_get (&c, 0, &value, &valid) && valid && value == 3.5);
  assert (mulib_columnar_get (&c, 1, &value, &valid) && !valid);
  mulib_columnar_free (&c);
  return 0;
}
