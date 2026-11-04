#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_SECURITY
#define MULIB_IMPL_OTPAUTH
#define MULIB_IMPL_OTPATH
#include "mulib-otpath.h"
#include <assert.h>
int main (void)
{
  /* Fixed test key only. Use an OS CSPRNG for production credentials. */
  mulib_otpauth_config_t c={"12345678901234567890",20,MULIB_AUTHCRYPTO_SHA1,6,30,0};
  mulib_otpauth_replay_t replay={0,0};
  char code[7];
  assert(mulib_otpath_totp(&c,60,code,sizeof code));
  assert(mulib_otpath_totp_verify(&c,code,60,0,0,&replay));
  assert(!mulib_otpath_totp_verify(&c,code,60,0,0,&replay));
  return 0;
}
