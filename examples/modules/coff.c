#include "../binary-formats-common.h"
int main (void)
{
  mulib_buffer_t b = { 0 }; mulib_coff_t v; mulib_coff_section_t s;
  assert (mulib_coffbuilder_text (&b, "\xc3", 1));
  assert (!mulib_coff_open (&v, b.data, b.length));
  assert (!mulib_coff_section (&v, 0, &s) && s.size == 1);
  mulib_buffer_free (&b); return 0;
}
