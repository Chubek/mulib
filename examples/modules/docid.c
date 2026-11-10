#define MULIB_IMPL_DOCID
#include "mulib-docid.h"
#include <assert.h>
int
main (void)
{
  unsigned char a[8], b[8];
  mulib_docid_encode (255, a);
  mulib_docid_encode (256, b);
  assert (memcmp (a, b, 8) < 0 && mulib_docid_decode (b) == 256);
  return 0;
}
