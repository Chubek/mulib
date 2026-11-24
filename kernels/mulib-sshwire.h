/** @file mulib-sshwire.h
 * @brief Bounded RFC 4251 wire primitives. Define MULIB_IMPL_SSHWIRE and ENDIAN.
 */
#ifndef MULIB_SSHWIRE_H
#define MULIB_SSHWIRE_H
#include "mulib-endian.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  const unsigned char *data;
  size_t length;
} mulib_ssh_slice_t;

typedef struct {
  const unsigned char *next;
  size_t left;
} mulib_ssh_reader_t;

typedef struct {
  unsigned char *next;
  size_t left;
} mulib_ssh_writer_t;

/* Primitive operations return 1/0 and commit cursors only on success.
 * Read slices borrow the input. NULL data is allowed for an empty slice. */
int mulib_ssh_read_bytes (mulib_ssh_reader_t *, size_t, mulib_ssh_slice_t *);
int mulib_ssh_read_u8 (mulib_ssh_reader_t *, unsigned char *);
int mulib_ssh_read_u32 (mulib_ssh_reader_t *, uint32_t *);
int mulib_ssh_read_u64 (mulib_ssh_reader_t *, uint64_t *);
int mulib_ssh_read_string (mulib_ssh_reader_t *, mulib_ssh_slice_t *);
/* Decode a canonical nonnegative mpint, omitting its sign byte. */
int mulib_ssh_read_mpint (mulib_ssh_reader_t *, mulib_ssh_slice_t *);
int mulib_ssh_write_bytes (mulib_ssh_writer_t *, const void *, size_t);
int mulib_ssh_write_u8 (mulib_ssh_writer_t *, unsigned char);
int mulib_ssh_write_u32 (mulib_ssh_writer_t *, uint32_t);
int mulib_ssh_write_u64 (mulib_ssh_writer_t *, uint64_t);
int mulib_ssh_write_string (mulib_ssh_writer_t *, const void *, size_t);
/* Encode a nonnegative big-endian magnitude as a canonical SSH mpint. */
int mulib_ssh_write_mpint (mulib_ssh_writer_t *, const void *, size_t);
int mulib_ssh_slice_is (mulib_ssh_slice_t, const char *literal);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_SSHWIRE) && !defined(MULIB_SSHWIRE_IMPLEMENTED)
#define MULIB_SSHWIRE_IMPLEMENTED
#include <string.h>

int
mulib_ssh_read_bytes (mulib_ssh_reader_t *r, size_t length, mulib_ssh_slice_t *v)
{
  if (!r || !v || length > r->left || (!r->next && r->left)) return 0;
  v->data = r->next;
  v->length = length;
  if (length) r->next += length;
  r->left -= length;
  return 1;
}

int
mulib_ssh_read_u8 (mulib_ssh_reader_t *r, unsigned char *value)
{
  mulib_ssh_slice_t v;
  if (!value || !mulib_ssh_read_bytes (r, 1, &v)) return 0;
  *value = v.data[0];
  return 1;
}

int
mulib_ssh_read_u32 (mulib_ssh_reader_t *r, uint32_t *value)
{
  mulib_ssh_slice_t v;
  if (!value || !mulib_ssh_read_bytes (r, 4, &v)) return 0;
  *value = mulib_endian_load_be32 (v.data);
  return 1;
}

int
mulib_ssh_read_u64 (mulib_ssh_reader_t *r, uint64_t *value)
{
  mulib_ssh_slice_t v;
  if (!value || !mulib_ssh_read_bytes (r, 8, &v)) return 0;
  *value = mulib_endian_load_be64 (v.data);
  return 1;
}

int
mulib_ssh_read_string (mulib_ssh_reader_t *r, mulib_ssh_slice_t *v)
{
  if (!r) return 0;
  mulib_ssh_reader_t cursor = *r;
  uint32_t length;
  if (!mulib_ssh_read_u32 (&cursor, &length)
      || !mulib_ssh_read_bytes (&cursor, length, v)) return 0;
  *r = cursor;
  return 1;
}

int
mulib_ssh_read_mpint (mulib_ssh_reader_t *r, mulib_ssh_slice_t *v)
{
  if (!r || !v) return 0;
  mulib_ssh_reader_t cursor = *r;
  mulib_ssh_slice_t magnitude;
  if (!mulib_ssh_read_string (&cursor, &magnitude)) return 0;
  if (magnitude.length) {
    if (magnitude.data[0] & 0x80) return 0;
    if (!magnitude.data[0]) {
      if (magnitude.length < 2 || !(magnitude.data[1] & 0x80)) return 0;
      ++magnitude.data;
      --magnitude.length;
    }
  }
  *r = cursor;
  *v = magnitude;
  return 1;
}

int
mulib_ssh_write_bytes (mulib_ssh_writer_t *w, const void *data, size_t length)
{
  if (!w || length > w->left || (!w->next && w->left) || (!data && length))
    return 0;
  if (length) {
    memmove (w->next, data, length);
    w->next += length;
    w->left -= length;
  }
  return 1;
}

int
mulib_ssh_write_u8 (mulib_ssh_writer_t *w, unsigned char value)
{
  return mulib_ssh_write_bytes (w, &value, 1);
}

int
mulib_ssh_write_u32 (mulib_ssh_writer_t *w, uint32_t value)
{
  unsigned char bytes[4];
  mulib_endian_store_be32 (bytes, value);
  return mulib_ssh_write_bytes (w, bytes, sizeof bytes);
}

int
mulib_ssh_write_u64 (mulib_ssh_writer_t *w, uint64_t value)
{
  unsigned char bytes[8];
  mulib_endian_store_be64 (bytes, value);
  return mulib_ssh_write_bytes (w, bytes, sizeof bytes);
}

int
mulib_ssh_write_string (mulib_ssh_writer_t *w, const void *data, size_t length)
{
  if (!w || !w->next || w->left < 4 || length > w->left - 4
      || length > UINT32_MAX || (!data && length)) return 0;
  if (length) memmove (w->next + 4, data, length);
  mulib_endian_store_be32 (w->next, (uint32_t)length);
  w->next += 4 + length;
  w->left -= 4 + length;
  return 1;
}

int
mulib_ssh_write_mpint (mulib_ssh_writer_t *w, const void *data, size_t length)
{
  const unsigned char *p = (const unsigned char *)data;
  if (!p && length) return 0;
  while (length && *p == 0) {
    ++p;
    --length;
  }
  if (!length || !(p[0] & 0x80)) return mulib_ssh_write_string (w, p, length);
  if (!w || !w->next || w->left < 5 || length > w->left - 5
      || length >= UINT32_MAX) return 0;
  memmove (w->next + 5, p, length);
  mulib_endian_store_be32 (w->next, (uint32_t)(length + 1));
  w->next[4] = 0;
  w->next += 5 + length;
  w->left -= 5 + length;
  return 1;
}

int
mulib_ssh_slice_is (mulib_ssh_slice_t v, const char *literal)
{
  return literal && strlen (literal) == v.length
         && (!v.length || (v.data && !memcmp (v.data, literal, v.length)));
}
#endif
