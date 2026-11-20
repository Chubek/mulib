#define MULIB_IMPL_XMACIR
#define MULIB_IMPL_CAPASM
#include "mulib-capasm.h"

#include <stdio.h>
#include <stdlib.h>

int main(void)
{
  static const char source[] =
    "XM_U32(portable_answer)\n"
    "XM_PUSH(6)\n"
    "XM_PUSH(7)\n"
    "XM_MUL()\n"
    "XM_RET()\n";
  mulib_xmacir_program_t program = { 0 };
  mulib_xmacir_error_t error = { 0, NULL };
  char *out = NULL;
  size_t length = 0;
  int ok = mulib_xmacir_parse(source, sizeof source - 1, &program, &error) &&
    mulib_capasm_emit(&program, NULL, &out, &length, &error);
  if (ok)
    ok = fwrite(out, 1, length, stdout) == length && fflush(stdout) == 0;
  else
    fprintf(stderr, "%zu: %s\n", error.offset, error.message ? error.message : "CAPASM failed");
  free(out);
  mulib_xmacir_free(&program);
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
