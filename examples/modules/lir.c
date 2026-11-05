#define MULIB_IMPL_MIR
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_MACHINE
#define MULIB_IMPL_LIR
#define MULIB_IMPL_LOWOPT
#include "mulib-lir.h"
#include <assert.h>
#include <stdio.h>
static int output (void *user, const char *text, size_t n)
{ return fwrite (text, 1, n, (FILE *)user) == n; }
int main (void)
{
  mulib_mir_native_instruction_t code[] = {
    {MULIB_MIR_NATIVE_CONST,0,SIZE_MAX,SIZE_MAX,SIZE_MAX,42},
    {MULIB_MIR_NATIVE_RETURN,SIZE_MAX,0,SIZE_MAX,SIZE_MAX,0}
  };
  mulib_mir_native_block_t block = {0,2};
  mulib_mir_native_program_t p = {code,2,&block,1,1,0,NULL,0,NULL,0};
  mulib_lir_t lir = {0};
  assert (mulib_lir_from_mir (&p, mulib_machine_x86_64_sysv (), 128, &lir, NULL) == MULIB_MIR_OK);
  lir.annotations[0].constraints = MULIB_LIR_KEEP;
  assert (mulib_lir_write (&lir, 128, output, stdout) == MULIB_MIR_OK);
  mulib_lir_free (&lir); return 0;
}
