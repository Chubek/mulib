/* Deterministic hostile inputs for the new bounded borrowed codecs. */
#define MULIB_IMPL_WAL
#define MULIB_IMPL_SSTFILE
#define MULIB_IMPL_MEMTABLE
#define MULIB_IMPL_BUFFER
#define MULIB_IMPL_BYCUTILS
#define MULIB_IMPL_CRC32
#define MULIB_IMPL_BINFILE
#define MULIB_IMPL_FLATBUFFERS
#define MULIB_IMPL_CONTAINER2DAEMONPROTO
#define MULIB_IMPL_OBJFILELIBSEARCH
#include "mulib-container2daemonproto.h"
#include "mulib-flatbuffers.h"
#include "mulib-objfilelibsearch.h"
#include "mulib-sstfile.h"
#include <assert.h>
#include <string.h>
static uint32_t random_state = 12345;
static unsigned char
byte (void)
{
  random_state ^= random_state << 13;
  random_state ^= random_state >> 17;
  random_state ^= random_state << 5;
  return (unsigned char)random_state;
}
int
main (void)
{
  unsigned char bytes[256];
  size_t i, j, used;
  uint64_t number;
  for (i = 0; i < 2000; ++i)
    {
      size_t n = i % sizeof (bytes);
      mulib_memtable_t t = { 0 };
      mulib_wal_reader_t r = { 0 };
      mulib_wal_record_t record;
      mulib_flatbuffers_table_t table;
      mulib_container2daemonproto_frame_t frame;
      mulib_binfile_t span;
      for (j = 0; j < n; ++j)
        bytes[j] = byte ();
      if (i % 4 == 0 && n >= 20)
        {
          memcpy (bytes, "MCDP\1\4\0\0", 8);
        }
      if (i % 4 == 1 && n >= 25)
        {
          memcpy (bytes, "MWAL", 4);
          memset (bytes + 20, 0, 5);
        }
      r.data = bytes;
      r.size = n;
      (void)mulib_wal_next (&r, &record);
      (void)mulib_sstfile_decode (bytes, n, &t);
      mulib_memtable_free (&t);
      if (mulib_flatbuffers_root (bytes, n, &table))
        {
          (void)mulib_flatbuffers_uint (&table, 0, 8, &number);
          (void)mulib_flatbuffers_string (&table, 0, &span);
        }
      (void)mulib_container2daemonproto_decode (bytes, n, &frame, &used);
      (void)mulib_objfilelibsearch_find (bytes, n, "x.o", &span);
    }
  return 0;
}
