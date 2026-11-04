#include "../binary-formats-common.h"
int main (void)
{
  mulib_buffer_t b = { 0 }; mulib_pe_t v; mulib_pe_section_t s;
  assert (mulib_pebuilder_text (&b, "\xc3", 1, 0));
  assert (!mulib_pe_open (&v, b.data, b.length));
  assert (!mulib_peutils_section (&v, ".text", NULL, &s) && s.address == 4096);
  mulib_buffer_free (&b); return 0;
}
