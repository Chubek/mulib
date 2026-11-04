#include "../binary-formats-common.h"
int main (void)
{
  mulib_elfbuilder_t builder = { 0 }; mulib_buffer_t b = { 0 }; mulib_strbuf_t text = { 0 };
  assert (mulib_elfbuilder_add (&builder, ".text", "\xc3", 1, 16, 6));
  assert (mulib_elfbuilder_finish (&builder, 62, &b));
  assert (mulib_elf_objdump (b.data, b.length, NULL, &text)); puts (text.data);
  mulib_elfbuilder_free (&builder); mulib_buffer_free (&b); mulib_strbuf_free (&text); return 0;
}
