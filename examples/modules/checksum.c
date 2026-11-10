#define MULIB_IMPL_CHECKSUM
#include "mulib-checksum.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
  mulib_checksum_ctx ctx;
  assert(mulib_checksum_init(&ctx,MULIB_CHECKSUM_ADLER32));
  assert(mulib_checksum_update(&ctx,"Wiki",4));
  assert(mulib_checksum_update(&ctx,"pedia",5));
  assert(mulib_checksum_final(&ctx)==UINT32_C(0x11e60398));
  printf("%04x\n",(unsigned)mulib_fletcher16("abcde",5)); return 0;
}
