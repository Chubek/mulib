#include "../binary-formats-common.h"
int main (void)
{
  mulib_buffer_t b = { 0 }; mulib_strbuf_t text = { 0 };
  assert (mulib_coffbuilder_text (&b, "\xc3", 1));
  assert (mulib_coff_objdump (b.data, b.length, NULL, &text)); puts (text.data);
  mulib_buffer_free (&b); mulib_strbuf_free (&text); return 0;
}
