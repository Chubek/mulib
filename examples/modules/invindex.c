#define MULIB_IMPL_INVINDEX
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#include "mulib-invindex.h"
#include <assert.h>
int
main (void)
{
  mulib_invindex_t t = { 0 };
  uint64_t doc;
  assert (!mulib_invindex_add (&t, "term", 4, 9)
          && !mulib_invindex_add (&t, "term", 4, 3));
  assert (!mulib_invindex_at (&t, "term", 4, 0, &doc) && doc == 3);
  mulib_invindex_free (&t);
  return 0;
}
