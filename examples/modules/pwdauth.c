#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_SECURITY
#define MULIB_IMPL_PWDAUTH
#include "mulib-pwdauth.h"
#include <assert.h>
int main (void)
{
  /* Deterministic EXAMPLE salt only. Production MUST use a fresh OS CSPRNG salt. */
  const unsigned char salt[]={0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
  mulib_pwdauth_record_t record;
  assert(mulib_pwdauth_create("example-password",16,salt,MULIB_PWDAUTH_MIN_ITERATIONS,&record));
  assert(mulib_pwdauth_verify(&record,"example-password",16));
  mulib_authcrypto_wipe(&record,sizeof record);
  return 0;
}
