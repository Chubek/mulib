#define MULIB_IMPL_TSV
#include "mulib-tsv.h"
#include <assert.h>
int
main (void)
{
  mulib_tsv_t row;
  const char *field;
  size_t n;
  mulib_tsv_init (&row, "Ada\t\t", 5);
  assert (mulib_tsv_next (&row, &field, &n) && n == 3);
  assert (mulib_tsv_next (&row, &field, &n) && n == 0);
  assert (mulib_tsv_next (&row, &field, &n) && n == 0);
  assert (!mulib_tsv_next (&row, &field, &n));
  return 0;
}
