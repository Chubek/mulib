#define MULIB_IMPL_QUIC
#define MULIB_IMPL_BUFFER
#include "mulib-quic.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t wire = { 0 };
  uint64_t value;
  size_t used;
  assert (mulib_quic_varint_write (&wire, 16384));
  assert (mulib_quic_varint_read (wire.data, wire.length, &value, &used) == 1
          && value == 16384 && used == 4);
  mulib_buffer_free (&wire);
  return 0;
}
