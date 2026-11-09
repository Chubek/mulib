#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_AUTHCRYPTO
#define MULIB_IMPL_ADLER32
#define MULIB_IMPL_DEXCC
#include "mulib-dexcc.h"
#include <assert.h>
#include <stdio.h>
/* An arbitrary IR can select/register-allocate its operations in this hook. */
static int lower (void *user, size_t index, mulib_dexcc_instruction_t *out)
{
  *out = mulib_dexcc_instruction (index ? MULIB_DEXCC_RETURN : MULIB_DEXCC_CONST);
  if (!index) out->literal = *(const int32_t *)user;
  return 1;
}
int main (int argc, char **argv)
{
  int32_t answer = 42;
  mulib_dexcc_ir_t ir = {2,1,0,lower,&answer};
  mulib_dexcc_buffer_t method = {0}; mulib_buffer_t dex = {0};
  assert (mulib_dexcc_import (&ir,100000,&method,NULL) == MULIB_DEXCC_OK);
  assert (mulib_dexcc_file (&method.method,"Lexample/Answer;","answer",100000,&dex,NULL) == MULIB_DEXCC_OK);
  assert (mulib_dexcc_check_file (dex.data,dex.length,100000,NULL) == MULIB_DEXCC_OK);
  if (argc == 2) {
    FILE *f = fopen (argv[1],"wb"); assert (f);
    assert (fwrite (dex.data,1,dex.length,f) == dex.length); assert (!fclose (f));
  }
  printf ("DEX035: %zu bytes\n", dex.length);
  mulib_dexcc_free (&method); mulib_buffer_free (&dex); return 0;
}
