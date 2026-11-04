#include "../binary-formats-common.h"
int main (void)
{
  mulib_buffer_t b = { 0 }; mulib_coff_t v; mulib_coff_section_t s; size_t i;
  assert (mulib_coffbuilder_text (&b, "\xc3", 1));
  assert (!mulib_coff_open (&v, b.data, b.length));
  assert (!mulib_coffutils_section (&v, ".text", &i, &s) && i == 0);
  mulib_buffer_free (&b); return 0;
}
