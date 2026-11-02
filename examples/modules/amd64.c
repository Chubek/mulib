#define MULIB_IMPL_AMD64
#define MULIB_IMPL_ASSEMBLER
#define MULIB_IMPL_DSYMSPEC
#define MULIB_IMPL_JSON
#define MULIB_IMPL_PARSEC
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#define MULIB_IMPL_EMITTER
#define MULIB_IMPL_LINKER
#include "mulib-amd64.h"
#include <assert.h>
int
main (void)
{
  mulib_amd64_t context = { 0 };
  mulib_emitter_t image;
  unsigned char code[16];
  assert (mulib_amd64_init (&context));
  assert (
      !mulib_emitter_init (&image, code, sizeof (code), MULIB_EMITTER_LITTLE));
  assert (!mulib_amd64_emit (&context, &image, NULL, "ret", NULL, 0, NULL));
  assert (image.size == 1 && code[0] == 0xc3);
  mulib_amd64_free (&context);
  return 0;
}
