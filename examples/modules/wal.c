#define MULIB_IMPL_WAL
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#include "mulib-wal.h"
#include <assert.h>
int
main (void)
{
  mulib_buffer_t b = { 0 };
  mulib_wal_reader_t r = { 0 };
  mulib_wal_record_t record;
  assert (!mulib_wal_append (&b, "k", 1, "v", 1, 1, 0));
  r.data = b.data;
  r.size = b.length;
  assert (mulib_wal_next (&r, &record) == MULIB_WAL_RECORD
          && record.sequence == 1);
  assert (!mulib_wal_next (&r, &record));
  mulib_buffer_free (&b);
  return 0;
}
