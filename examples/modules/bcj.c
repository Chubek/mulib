#define MULIB_IMPL_BCJ
#include "mulib-bcj.h"
#include <assert.h>
#include <string.h>
int main (void)
{
  unsigned char branch[4]={0,0,0,235},original[4];
  memcpy (original,branch,4);
  assert (!mulib_bcj_transform (7,branch,4,0,1));
  assert (memcmp (branch,original,4));
  assert (!mulib_bcj_transform (7,branch,4,0,0));
  assert (!memcmp (branch,original,4)); return 0;
}
