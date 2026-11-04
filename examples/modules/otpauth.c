#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_SECURITY
#define MULIB_IMPL_OTPAUTH
#include "mulib-otpauth.h"
#include <assert.h>
#include <string.h>
int main (void)
{
  /* RFC test secret, NOT suitable for real accounts. Persist replay per user. */
  mulib_otpauth_config_t c={"12345678901234567890",20,MULIB_AUTHCRYPTO_SHA1,8,30,0};
  mulib_otpauth_replay_t replay={0,0};
  char code[9];
  assert(mulib_otpauth_totp(&c,59,code,sizeof code));
  assert(!strcmp(code,"94287082"));
  assert(mulib_otpauth_totp_verify(&c,code,59,1,1,&replay));
  assert(!mulib_otpauth_totp_verify(&c,code,59,1,1,&replay));
  return 0;
}
