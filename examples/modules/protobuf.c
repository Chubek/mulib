#define MULIB_IMPL_PROTOBUF
#define MULIB_IMPL_VARINT
#define MULIB_IMPL_BUFFER
#include "mulib-protobuf.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t wire = { 0 };
  mulib_protobuf_field_t field = { 1, 0, 150, NULL, 0 }, copy;
  size_t used;
  assert (mulib_protobuf_write (&wire, &field));
  assert (wire.length == 3 && !memcmp (wire.data, "\x08\x96\x01", 3));
  assert (mulib_protobuf_read (wire.data, wire.length, &copy, &used) == 1
          && copy.value == 150);
  mulib_buffer_free (&wire);
  return 0;
}
