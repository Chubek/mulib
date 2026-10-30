#define MULIB_IMPL_HEAPTABLE
#define MULIB_IMPL_MEMTABLE
#include "mulib-heaptable.h"
#include <assert.h>
int
main (void)
{
  mulib_heaptable_t t = { 0 };
  uint64_t id;
  const void *data;
  size_t n;
  assert (!mulib_heaptable_insert (&t, "row", 3, &id));
  assert (!mulib_heaptable_get (&t, id, &data, &n) && n == 3);
  mulib_heaptable_free (&t);
  return 0;
}
