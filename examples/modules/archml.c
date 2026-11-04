#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_ARCHML
#include "mulib-archml.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main (void)
{
  const char source[] =
    "(archml 1 tiny (target tiny (bits 32) (endian little))"
    " (pcode-op nop (arity 0 0) (semantics \"Advance PC.\"))"
    " (instruction nop (operands) (encoding (base \"90\")) (pcode (nop))))";
  mulib_archml_t *a = NULL;
  mulib_archml_error_t error;
  unsigned char byte = 0;
  size_t used = 0;
  const char *name;
  assert (mulib_archml_parse (source, strlen (source), NULL, &a, &error) == MULIB_ARCHML_OK);
  assert (mulib_archml_encode (a, "nop", NULL, 0, &byte, 1, &used) == MULIB_ARCHML_OK);
  assert (byte == 0x90 && used == 1);
  name = mulib_archml_name (a);
  assert (name);
  printf ("%s: %zu concrete instruction\n", name, mulib_archml_count (a, "instruction"));
  mulib_archml_destroy (a);
  return 0;
}
