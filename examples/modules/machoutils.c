#include "../binary-formats-common.h"
int main (void)
{
  mulib_buffer_t b = { 0 }; mulib_macho_t v; mulib_macho_section_t s;
  assert (mulib_machobuilder_text (&b, "\xc3", 1));
  assert (!mulib_macho_open (&v, b.data, b.length));
  assert (!mulib_machoutils_section (&v, "__TEXT", "__text", NULL, &s));
  mulib_buffer_free (&b); return 0;
}
