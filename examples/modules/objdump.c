#define MULIB_IMPL_STRBUF
#define MULIB_IMPL_TERMINFO
#define MULIB_IMPL_TERMSTYLE
#define MULIB_IMPL_OBJDUMP
#include "mulib-objdump.h"
#include <assert.h>
int main (void)
{
  mulib_strbuf_t text = { 0 };
  assert (mulib_objdump_hex (&text, "hello\0", 6, 0, 256));
  assert (strstr (text.data, "68 65 6c 6c 6f 00"));
  puts (text.data); mulib_strbuf_free (&text); return 0;
}
