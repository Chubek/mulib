#include "../binary-formats-common.h"
int main (void)
{
  mulib_buffer_t b = { 0 }; mulib_strbuf_t text = { 0 };
  assert (mulib_pebuilder_text (&b, "\xc3", 1, 0));
  assert (mulib_pe_objdump (b.data, b.length, NULL, &text)); puts (text.data);
  mulib_buffer_free (&b); mulib_strbuf_free (&text); return 0;
}
