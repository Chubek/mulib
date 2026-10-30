/**
 * @file mulib-wal.h
 * @brief CRC-framed, sequence-checked write-ahead-log byte codec, not disk
 * I/O. Define MULIB_IMPL_WAL plus BUFFER, BYCUTILS and CRC32 dependency
 * switches.
 */
#ifndef MULIB_WAL_H
#define MULIB_WAL_H
#include "mulib-buffer.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_WAL_OK = 0,
    MULIB_WAL_RECORD = 1,
    MULIB_WAL_TORN = 2,
    MULIB_WAL_EINVAL = -1,
    MULIB_WAL_ECORRUPT = -2,
    MULIB_WAL_ENOMEM = -3
  };
  typedef struct mulib_wal_record
  {
    const unsigned char *key, *value;
    size_t key_size, value_size;
    uint64_t sequence;
    int deleted;
  } mulib_wal_record_t;
  /** @brief Borrowed input. Initialize all fields to zero, then set data/size.
   * last_sequence enforces strictly increasing nonzero sequences. */
  typedef struct mulib_wal_reader
  {
    const unsigned char *data;
    size_t size, offset;
    uint64_t last_sequence;
  } mulib_wal_reader_t;
  /** @brief Append a complete frame atomically. Caller enforces append
   * sequence. Records use 32-bit lengths; input must not alias log storage. */
  int mulib_wal_append (mulib_buffer_t *log, const void *key, size_t kn,
                        const void *value, size_t vn, uint64_t sequence,
                        int deleted);
  /** @brief RECORD advances; OK means clean EOF. TORN means incomplete final
   * frame. Other errors mean corruption. Cursor/output unchanged except on
   * RECORD. Never skip a corrupt frame; persistence, fsync and tail truncation
   * are caller policy. */
  int mulib_wal_next (mulib_wal_reader_t *reader, mulib_wal_record_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_WAL) && !defined(MULIB_WAL_IMPLEMENTED)
#define MULIB_WAL_IMPLEMENTED
#include "mulib-bycutils.h"
#include "mulib-crc32.h"
#include <string.h>
int
mulib_wal_append (mulib_buffer_t *b, const void *k, size_t kn, const void *v,
                  size_t vn, uint64_t seq, int del)
{
  mulib_buffer_t tmp = { 0 };
  unsigned char h[25] = { 0 }, crc[4];
  int ok;
  if (!b || (!k && kn) || (!v && vn) || kn > UINT32_MAX || vn > UINT32_MAX
      || !seq || (del != 0 && del != 1) || (del && vn))
    return -1;
  memcpy (h, "MWAL", 4);
  mulib_bycutils_store (h, sizeof (h), 4, 4, kn);
  mulib_bycutils_store (h, sizeof (h), 8, 4, vn);
  mulib_bycutils_store (h, sizeof (h), 12, 8, seq);
  h[20] = (unsigned char)del;
  ok = mulib_buffer_append (&tmp, h, sizeof (h))
       && mulib_buffer_append (&tmp, k, kn)
       && mulib_buffer_append (&tmp, v, vn);
  if (ok)
    {
      mulib_bycutils_store (crc, 4, 0, 4, mulib_crc32 (tmp.data, tmp.length));
      ok = mulib_buffer_append (&tmp, crc, 4)
           && mulib_buffer_append (b, tmp.data, tmp.length);
    }
  mulib_buffer_free (&tmp);
  return ok ? 0 : MULIB_WAL_ENOMEM;
}
int
mulib_wal_next (mulib_wal_reader_t *r, mulib_wal_record_t *out)
{
  uint64_t kn, vn, seq, crc;
  size_t n, total;
  const unsigned char *p;
  mulib_wal_record_t v;
  if (!r || !out || (!r->data && r->size) || r->offset > r->size)
    return -1;
  n = r->size - r->offset;
  if (!n)
    return 0;
  if (n < 25)
    return MULIB_WAL_TORN;
  p = r->data + r->offset;
  if (memcmp (p, "MWAL", 4) || p[20] > 1 || p[21] || p[22] || p[23] || p[24])
    return -2;
  mulib_bycutils_load (p, n, 4, 4, &kn);
  mulib_bycutils_load (p, n, 8, 4, &vn);
  mulib_bycutils_load (p, n, 12, 8, &seq);
  if (!seq || seq <= r->last_sequence || (p[20] && vn))
    return -2;
  if (kn > SIZE_MAX - 29 || vn > SIZE_MAX - 29 - (size_t)kn)
    return -2;
  total = 29 + (size_t)kn + (size_t)vn;
  if (total > n)
    return MULIB_WAL_TORN;
  if (!mulib_bycutils_load (p, total, total - 4, 4, &crc)
      || crc != mulib_crc32 (p, total - 4))
    return -2;
  v.key = p + 25;
  v.key_size = (size_t)kn;
  v.value = v.key + v.key_size;
  v.value_size = (size_t)vn;
  v.sequence = seq;
  v.deleted = p[20];
  *out = v;
  r->offset += total;
  r->last_sequence = seq;
  return MULIB_WAL_RECORD;
}
#endif
