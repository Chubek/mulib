/** @file mulib-http3.h
 * @brief Bounded HTTP/3 frame and SETTINGS payload validation, RFC 9114.
 * Define MULIB_IMPL_HTTP3, QUIC and BUFFER. Unknown frame types are preserved.
 * No QPACK, stream-role/connection state machine or QUIC transport.
 * Parse returns 1 OK, 0 MORE, -1 invalid, -2 limit, -3 OOM; failure unchanged. */
#ifndef MULIB_HTTP3_H
#define MULIB_HTTP3_H
#include "mulib-quic.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_http3_frame
  {
    uint64_t type;
    const unsigned char *data;
    size_t length;
  } mulib_http3_frame_t;
  /** Read one frame with an explicit payload cap; zero permits empty only.
   * Known frame payloads are structurally checked; incomplete varints INSIDE
   * a complete frame are invalid, not MORE. SETTINGS rejects duplicate IDs,
   * HTTP/2-reserved IDs and invalid standardized boolean settings. Unknown
   * extension frames/settings and nonminimal QUIC varints remain accepted.
   * Both output arguments are unchanged unless successful. SETTINGS with more
   * than 32 entries needs a temporary ID array (8 bytes/entry, O(n log n) work).
   */
  int mulib_http3_frame_read (const void *data, size_t length,
                              size_t max_payload, mulib_http3_frame_t *out,
                              size_t *consumed);
  /** Append a validated frame transactionally; 1 success, 0 invalid/OOM.
   * out must be initialized. Payload may alias its existing bytes. */
  int mulib_http3_frame_write (mulib_buffer_t *out,
                               const mulib_http3_frame_t *frame);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HTTP3) && !defined(MULIB_HTTP3_IMPLEMENTED)
#define MULIB_HTTP3_IMPLEMENTED
#include <stdlib.h>
static int
mulib_http3_compare_ (const void *a, const void *b)
{
  uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
  return x < y ? -1 : x > y;
}
static int
mulib_http3_settings_ (const unsigned char *p, size_t n)
{
  uint64_t local[32], *ids = local, id, value;
  size_t at = 0, k, count = 0;
  int result = 1;
  /* Validate and count before allocating; malformed payloads cannot request a
   * large temporary allocation merely through their advertised byte length. */
  while (at < n) {
    if (mulib_quic_varint_read (p + at, n - at, &id, &k) != 1) return -1;
    at += k;
    if (mulib_quic_varint_read (p + at, n - at, &value, &k) != 1) return -1;
    at += k;
    if ((id >= 2 && id <= 5) || ((id == 8 || id == 0x33) && value > 1)) return -1;
    ++count;
  }
  if (count > sizeof (local) / sizeof (*local)) {
    if (count > SIZE_MAX / sizeof (*ids)) return -2;
    ids = (uint64_t *)malloc (count * sizeof (*ids));
    if (!ids) return -3;
  }
  at = 0;
  for (size_t i = 0; i < count; ++i) {
    (void)mulib_quic_varint_read (p + at, n - at, ids + i, &k); at += k;
    (void)mulib_quic_varint_read (p + at, n - at, &value, &k); at += k;
  }
  qsort (ids, count, sizeof (*ids), mulib_http3_compare_);
  for (size_t i = 1; i < count; ++i)
    if (ids[i - 1] == ids[i]) { result = -1; break; }
  if (ids != local) free (ids);
  return result;
}
static int
mulib_http3_valid_ (const mulib_http3_frame_t *f)
{
  uint64_t id;
  size_t used;
  if ((!f->data && f->length) || f->type > MULIB_QUIC_MAX_INTEGER
      || f->length > MULIB_QUIC_MAX_INTEGER) return -1;
  switch (f->type) {
  case 2: case 6: case 8: case 9: /* Reserved HTTP/2 frame types. */
    return -1;
  case 4:
    return mulib_http3_settings_ (f->data, f->length);
  case 3: case 7: case 13:
    return mulib_quic_varint_read (f->data, f->length, &id, &used) == 1
           && used == f->length ? 1 : -1;
  case 5:
    return mulib_quic_varint_read (f->data, f->length, &id, &used) == 1 ? 1 : -1;
  default:
    return 1;
  }
}
int
mulib_http3_frame_read (const void *p, size_t n, size_t max,
                        mulib_http3_frame_t *out, size_t *used)
{
  const unsigned char *s = (const unsigned char *)p;
  mulib_http3_frame_t f;
  uint64_t len;
  size_t at, k;
  int rc;
  if (!out || !used || (!p && n))
    return -1;
  rc = mulib_quic_varint_read (p, n, &f.type, &at);
  if (rc != 1)
    return rc;
  rc = mulib_quic_varint_read (s + at, n - at, &len, &k);
  if (rc != 1)
    return rc;
  at += k;
  if (len > max)
    return -2;
  if (len > n - at)
    return 0;
  f.data = s + at;
  f.length = (size_t)len;
  rc = mulib_http3_valid_ (&f);
  if (rc != 1) return rc;
  *out = f;
  *used = at + f.length;
  return 1;
}
int
mulib_http3_frame_write (mulib_buffer_t *out, const mulib_http3_frame_t *f)
{
  mulib_buffer_t b = { 0 };
  int ok;
  if (!out || !f || mulib_http3_valid_ (f) != 1)
    return 0;
  ok = mulib_quic_varint_write (&b, f->type)
       && mulib_quic_varint_write (&b, f->length)
       && mulib_buffer_append (&b, f->data, f->length)
       && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
#endif
