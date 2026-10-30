#define MULIB_IMPL_TEX
#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_UTF8
#include "mulib-tex.h"
#include <assert.h>
#include <string.h>
int
main (void)
{
  mulib_strbuf_t b = { 0 };
  assert (mulib_tex_escape (&b, "a_b", 3));
  assert (!strcmp (b.data, "a\\_b"));
  mulib_strbuf_free (&b);
  return 0;
}
