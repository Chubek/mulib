#define MULIB_IMPL_COLUMN_RDBMS
#include "mulib-column-rdbms.h"
#include <assert.h>
int
main (void)
{
  mulib_column_rdbms_t table = { NULL, 2, 0 };
  const double a[] = { 1, 2 }, b[] = { 3, 4 };
  assert (mulib_column_rdbms_append (&table, a, 2));
  assert (mulib_column_rdbms_append (&table, b, 2));
  assert (mulib_column_rdbms_column (&table, 1)[1] == 4);
  mulib_column_rdbms_free (&table);
  return 0;
}
