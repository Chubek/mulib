/* Link C++17 consumers against actual C11 implementation objects. */
#include "mulib-cryptohash.h"
#include "mulib-auxhash.h"
#include "mulib-crc.h"
#include "mulib-checksum.h"
#include <cassert>
#include <cstring>
int main()
{
  unsigned char sha[32],blake[32],key[32]={},mac[16];
  assert(mulib_sha256("abc",3,sha));
  assert(sha[0]==0xba && sha[31]==0xad);
  mulib_sha_ctx c{};
  assert(mulib_sha_init_intrinsics(&c,MULIB_SHA256));
  assert(mulib_sha_update(&c,"abc",3));
  assert(mulib_sha_final(&c,blake,sizeof blake));
  assert(std::memcmp(sha,blake,32)==0);
  assert(mulib_blake2s("abc",3,nullptr,0,blake,32));
  assert(mulib_blake3("abc",3,blake,32));
  assert(mulib_kmac128(key,32,"abc",3,nullptr,0,blake,32));
  assert(mulib_siphash24(key,"abc",3,mac));
  assert(mulib_pmac(key,32,"abc",3,mac));
  assert(mulib_auxhash_fnv1a_32("hello",5)==0x4f9f2cab);
  assert(mulib_checksum_adler32("Wikipedia",9)==0x11e60398);
  assert(mulib_crc_32_iso_hdlc("123456789",9)==0xcbf43926);
  return 0;
}
