#define MULIB_IMPL_CRC
#include "mulib-crc.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
int main(void)
{
  mulib_crc_ctx ctx; mulib_crc_value crc;
  assert(mulib_crc_init(&ctx,mulib_crc_find("CRC-32/ISO-HDLC")));
  assert(mulib_crc_update(&ctx,"1234",4));
  assert(mulib_crc_update(&ctx,"56789",5)); crc=mulib_crc_final(&ctx);
  assert(crc.lo==UINT32_C(0xcbf43926));
  printf("%08" PRIx64 "\n",crc.lo); return 0;
}
