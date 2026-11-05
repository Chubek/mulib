#define MULIB_IMPL_MIR
#define MULIB_IMPL_MUBURG
#define MULIB_IMPL_MACHINE
#define MULIB_IMPL_LIR
#define MULIB_IMPL_ISCHED
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#define MULIB_IMPL_LOWOPT
#define MULIB_IMPL_ISEL
#define MULIB_IMPL_PEEPHOLE
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_SUPERINSTR
#define MULIB_IMPL_OPTML
#include "mulib-optml.h"
#include <assert.h>
#include <stdio.h>

static int write_text (void *user, const char *text, size_t length)
{ return fwrite (text, 1, length, (FILE *)user) == length; }
int main (int argc, char **argv)
{
  const size_t none = MULIB_MIR_NONE;
  mulib_mir_native_instruction_t code[] = {
    {MULIB_MIR_NATIVE_PARAM,0,0,none,none,0},
    {MULIB_MIR_NATIVE_CONST,1,none,none,none,2},
    {MULIB_MIR_NATIVE_MUL,2,0,1,none,0},
    {MULIB_MIR_NATIVE_RETURN,none,2,none,none,0}
  };
  mulib_mir_native_block_t block = {0,4};
  mulib_mir_native_program_t p = {code,4,&block,1,3,1,NULL,0,NULL,0};
  mulib_optml_options_t options = mulib_optml_defaults ();
  mulib_optml_result_t result = {0};
  mulib_archml_t *arch = NULL;
  mulib_mir_diagnostic_t d;
  /* Optional: examples/optml library/archml/amd64.aml > twice.s */
  if (argc > 1) {
    assert (mulib_archml_load (argv[1], NULL, &arch, NULL) == MULIB_ARCHML_OK);
    options.selection.arch = arch;
  }
  assert (mulib_optml_compile (&p, "twice", &options, &result, &d) == MULIB_MIR_OK);
  assert (result.stats.rewrites);
  assert (mulib_optml_write (&result, write_text, stdout) == MULIB_MIR_OK);
  mulib_optml_free (&result); mulib_archml_destroy (arch);
  return 0;
}
