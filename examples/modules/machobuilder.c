#include "../binary-formats-common.h"
int main (int argc, char **argv)
{
  mulib_machobuilder_t builder = { 0 }; mulib_buffer_t b = { 0 }; mulib_macho_t v;
  assert (mulib_machobuilder_add (&builder, "__text", "\xc3", 1, 16, 6));
  assert (mulib_machobuilder_add (&builder, "__data", "hello", 5, 8, 3));
  assert (mulib_machobuilder_finish (&builder, 0x01000007, &b));
  assert (!mulib_macho_open (&v, b.data, b.length) && v.section_count == 2);
  example_binary_write (argc, argv, b.data, b.length);
  mulib_machobuilder_free (&builder); mulib_buffer_free (&b); return 0;
}
