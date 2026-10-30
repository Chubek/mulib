#define MULIB_IMPL_MEMTABLE
#include "mulib-memtable.h"
#include <assert.h>
int
main (void)
{
  mulib_memtable_t t = { 0 };
  const mulib_memtable_record_t *r;
  assert (!mulib_memtable_put (&t, "key", 3, "value", 5, 1, 0));
  assert (!mulib_memtable_get (&t, "key", 3, &r) && r->value_size == 5);
  mulib_memtable_free (&t);
  return 0;
}
