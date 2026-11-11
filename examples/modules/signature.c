#define MULIB_IMPL_ENTROPY
#define MULIB_IMPL_SHA
#define MULIB_IMPL_DER
#define MULIB_IMPL_SIGNATURE
#include "mulib-signature.h"
#include <assert.h>
static void hex(const char *p,unsigned char *out,size_t n)
{for(size_t i=0;i<n;++i){unsigned a=(unsigned char)p[2*i],b=(unsigned char)p[2*i+1];out[i]=(unsigned char)((a<='9'?a-'0':a-'a'+10)*16+(b<='9'?b-'0':b-'a'+10));}}
int main(void)
{
  /* RFC 6979 A.2.5: P-256, SHA-256, message "sample". */
  unsigned char public_key[65],signature[72];mulib_public_key_t key={0};
  hex("0460fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb67903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299",public_key,65);
  hex("3046022100efd48b2aacb6a8fd1140dd9cd45e81d69d2c877b56aaf991c34d0ea84eaf3716022100f7cb1c942d657c41d436c7a1b6e29f65f3e900dbb9aff4064dc4ab2f843acda8",signature,72);
  key.kind=23;key.key=public_key;key.key_length=65;
  assert(mulib_signature_verify(&key,0x0403,"sample",6,signature,72));return 0;
}
