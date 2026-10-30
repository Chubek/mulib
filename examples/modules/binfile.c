#define MULIB_IMPL_BINFILE
#include "mulib-binfile.h"
#include <assert.h>
int
main (void)
{
  mulib_binfile_t f, s;
  assert (mulib_binfile_open (&f, "abcdef", 6));
  assert (mulib_binfile_slice (&f, 2, 3, &s) && s.size == 3
          && s.data[0] == 'c');
  assert (!mulib_binfile_slice (&f, 6, 1, &s));
  return 0;
}
