#define MULIB_IMPL_AMD64
#define MULIB_IMPL_MPU
#define MULIB_IMPL_UNIT
#define MULIB_IMPL_ARCHML
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
  {
    const uint64_t operands[] = {9, 12, (uint64_t)-16};
    assert (mulib_amd64_emit_form (&context, &image, "load64-base-disp32",
        operands, 3, NULL) == MULIB_ARCHML_OK);
    assert (image.size == 9 && code[1] == 0x4d && code[2] == 0x8b);
  }
  mulib_amd64_free (&context);
  return 0;
}
