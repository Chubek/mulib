#include "../binary-formats-common.h"
int main (void)
{
  mulib_buffer_t b = { 0 }; mulib_pe_t v; const unsigned char *entry;
  assert (mulib_pebuilder_text (&b, "\xc3", 1, 0));
  assert (!mulib_pe_open (&v, b.data, b.length) && v.bits == 64);
  assert (!mulib_pe_rva (&v, v.entry, 1, &entry) && *entry == 0xc3);
  mulib_buffer_free (&b); return 0;
}
