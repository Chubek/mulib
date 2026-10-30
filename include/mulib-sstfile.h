/**
 * @file mulib-sstfile.h
 * @brief Immutable sorted-table byte snapshots, preserving
 * tombstones/sequences. Define MULIB_IMPL_SSTFILE and MEMTABLE, WAL, BUFFER,
 * BYCUTILS, CRC32 switches.
 */
#ifndef MULIB_SSTFILE_H
#define MULIB_SSTFILE_H
#include "mulib-buffer.h"
#include "mulib-memtable.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Encode to an initialized buffer; appends one snapshot atomically.
   */
  int mulib_sstfile_encode (const mulib_memtable_t *table,
                            mulib_buffer_t *out);
  /** @brief Decode exactly one snapshot to a zero/uninitialized owned table.
   * Reject duplicate/unsorted keys, bad checksums and trailing/incomplete
   * bytes. Outputs unchanged on failure. Returns 0, negative on
   * invalid/corrupt/OOM. */
  int mulib_sstfile_decode (const void *data, size_t size,
                            mulib_memtable_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SSTFILE) && !defined(MULIB_SSTFILE_IMPLEMENTED)
#define MULIB_SSTFILE_IMPLEMENTED
#include "mulib-bycutils.h"
#include "mulib-crc32.h"
#include "mulib-wal.h"
#include <string.h>
int
mulib_sstfile_encode (const mulib_memtable_t *t, mulib_buffer_t *out)
{
  mulib_buffer_t b = { 0 };
  unsigned char h[16] = { 0 }, c[4];
  size_t i;
  int rc = 0;
  if (!t || !out)
    return -1;
  memcpy (h, "MuSST\1\0\0", 8);
  mulib_bycutils_store (h, 16, 8, 8, t->count);
  if (!mulib_buffer_append (&b, h, 16))
    return -3;
  for (i = 0; i < t->count && !rc; ++i)
    {
      const mulib_memtable_record_t *r = t->records + i;
      /* WAL frame sequence is ordinal; the original sequence is stored before
       * it. */
      unsigned char s[8];
      mulib_bycutils_store (s, 8, 0, 8, r->sequence);
      if (!mulib_buffer_append (&b, s, 8))
        rc = -3;
      else
        rc = mulib_wal_append (&b, r->key, r->key_size, r->value,
                               r->value_size, (uint64_t)i + 1, r->deleted);
    }
  if (!rc)
    {
      mulib_bycutils_store (c, 4, 0, 4, mulib_crc32 (b.data, b.length));
      if (!mulib_buffer_append (&b, c, 4)
          || !mulib_buffer_append (out, b.data, b.length))
        rc = -3;
    }
  mulib_buffer_free (&b);
  return rc;
}
int
mulib_sstfile_decode (const void *data, size_t n, mulib_memtable_t *out)
{
  const unsigned char *p = (const unsigned char *)data;
  uint64_t count, crc, seq;
  mulib_memtable_t t = { 0 };
  mulib_wal_reader_t r = { 0 };
  mulib_wal_record_t v;
  size_t i;
  int rc = -2;
  if (!p || !out || n < 20 || memcmp (p, "MuSST\1\0\0", 8))
    return -2;
  mulib_bycutils_load (p, n, n - 4, 4, &crc);
  if (crc != mulib_crc32 (p, n - 4))
    return -2;
  mulib_bycutils_load (p, n, 8, 8, &count);
  if (count > (n - 20) / 37)
    return -2;
  r.data = p;
  r.size = n - 4;
  r.offset = 16;
  for (i = 0; i < (size_t)count; ++i)
    {
      if (!mulib_bycutils_load (p, r.size, r.offset, 8, &seq))
        goto fail;
      r.offset += 8;
      if (mulib_wal_next (&r, &v) != MULIB_WAL_RECORD
          || v.sequence != (uint64_t)i + 1)
        goto fail;
      if (t.count
          && mulib_memtable_compare (t.records[t.count - 1].key,
                                     t.records[t.count - 1].key_size, v.key,
                                     v.key_size)
                 >= 0)
        goto fail;
      rc = mulib_memtable_put (&t, v.key, v.key_size, v.value, v.value_size,
                               seq, v.deleted);
      if (rc)
        goto fail;
      rc = -2;
    }
  if (r.offset != r.size)
    goto fail;
  *out = t;
  return 0;
fail:
  mulib_memtable_free (&t);
  return rc;
}
#endif
