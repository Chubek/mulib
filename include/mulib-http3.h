/** @file mulib-http3.h
 * @brief Bounded HTTP/3 frame and SETTINGS payload validation, RFC 9114.
 * Define MULIB_IMPL_HTTP3, QUIC and BUFFER. Unknown frame types are preserved.
 * Includes a bounded request/control stream state machine and literal QPACK
 * field sections. The caller supplies ordered, authenticated stream bytes.
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
  enum
  {
    MULIB_HTTP3_FRAME_UNEXPECTED = 0x105,
    MULIB_HTTP3_FRAME_ERROR = 0x106,
    MULIB_HTTP3_CLOSED_CRITICAL_STREAM = 0x104,
    MULIB_HTTP3_MISSING_SETTINGS = 0x10a,
    MULIB_HTTP3_REQUEST_INCOMPLETE = 0x10d,
    MULIB_HTTP3_ID_ERROR = 0x108,
    MULIB_HTTP3_SETTINGS_ERROR = 0x109
  };
  typedef struct mulib_http3_stream_state
  {
    unsigned state; /* Initialize using mulib_http3_stream_init. */
    unsigned error; /* HTTP/3 application error after a protocol failure. */
    uint64_t goaway_id;
    unsigned char control, peer_is_server, has_goaway;
  } mulib_http3_stream_state_t;
  /** State for one peer control stream or the request half of a request
   * stream. For a control stream, consume its type varint before using this API. */
  void mulib_http3_stream_init (mulib_http3_stream_state_t *stream,
                                 int control, int peer_is_server);
  /** Read and apply one frame. Returns the frame-reader status; -1 latches
   * a protocol error and leaves frame and consumed unchanged. */
  int mulib_http3_stream_next (mulib_http3_stream_state_t *stream,
                                const void *data, size_t length,
                                size_t max_payload, mulib_http3_frame_t *frame,
                                size_t *consumed);
  /** Signal FIN after consuming complete frames. partial is nonzero when
   * unconsumed bytes remain. Returns 1 on a clean FIN or -1 on protocol error. */
  int mulib_http3_stream_finish (mulib_http3_stream_state_t *stream, int partial);

  typedef struct mulib_http3_field
  {
    const unsigned char *name, *value;
    size_t name_length, value_length;
    int never_index;
  } mulib_http3_field_t;
  /** Write a QPACK field section with zero dynamic table capacity, using
   * only literal names and raw (non-Huffman) strings. Append atomically. */
  int mulib_http3_qpack_literal_write (mulib_buffer_t *out,
                                       const mulib_http3_field_t *fields,
                                       size_t count);
  /** Read a complete literal-only field section. Outputs borrow input and
   * are unchanged on failure. Returns 1 on success, -1 for malformed data,
   * -2 for capacity exhaustion, -4 for unsupported QPACK representations. */
  int mulib_http3_qpack_literal_read (const void *data, size_t length,
                                      mulib_http3_field_t *fields,
                                      size_t capacity, size_t *count);
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
enum
{
  MULIB_HTTP3_BEFORE_HEADERS_,
  MULIB_HTTP3_AFTER_HEADERS_,
  MULIB_HTTP3_AFTER_TRAILERS_,
  MULIB_HTTP3_FINISHED_
};

void
mulib_http3_stream_init (mulib_http3_stream_state_t *s, int control,
                         int peer_is_server)
{
  if (!s)
    return;
  memset (s, 0, sizeof (*s));
  s->control = control != 0;
  s->peer_is_server = peer_is_server != 0;
}

static int
mulib_http3_stream_error_ (mulib_http3_stream_state_t *s, unsigned error)
{
  s->error = error;
  return -1;
}

int
mulib_http3_stream_next (mulib_http3_stream_state_t *s, const void *data,
                         size_t n, size_t max, mulib_http3_frame_t *out,
                         size_t *consumed)
{
  mulib_http3_frame_t frame;
  size_t used, id_size;
  uint64_t id;
  int rc;
  if (!s || !out || !consumed || (!data && n) || s->error
      || s->state == MULIB_HTTP3_FINISHED_)
    return -1;
  rc = mulib_http3_frame_read (data, n, max, &frame, &used);
  if (rc == -1)
    {
      if (mulib_quic_varint_read (data, n, &id, &id_size) == 1)
        {
          if (id == 2 || id == 6 || id == 8 || id == 9)
            return mulib_http3_stream_error_ (s, MULIB_HTTP3_FRAME_UNEXPECTED);
          if (id == 4)
            {
              if (!s->control || s->state != MULIB_HTTP3_BEFORE_HEADERS_)
                return mulib_http3_stream_error_ (s, MULIB_HTTP3_FRAME_UNEXPECTED);
              return mulib_http3_stream_error_ (s, MULIB_HTTP3_SETTINGS_ERROR);
            }
        }
      return mulib_http3_stream_error_ (s, MULIB_HTTP3_FRAME_ERROR);
    }
  if (rc != 1)
    return rc;
  if (s->control)
    {
      if (s->state == MULIB_HTTP3_BEFORE_HEADERS_)
        {
          if (frame.type != 4)
            return mulib_http3_stream_error_ (s, MULIB_HTTP3_MISSING_SETTINGS);
          s->state = MULIB_HTTP3_AFTER_HEADERS_;
        }
      else if (frame.type == 0 || frame.type == 1 || frame.type == 4
               || frame.type == 5 || (frame.type == 13 && s->peer_is_server))
        return mulib_http3_stream_error_ (s, MULIB_HTTP3_FRAME_UNEXPECTED);
      else if (frame.type == 7)
        {
          (void)mulib_quic_varint_read (frame.data, frame.length, &id, &id_size);
          if ((s->peer_is_server && id % 4 != 0)
              || (s->has_goaway && id > s->goaway_id))
            return mulib_http3_stream_error_ (s, MULIB_HTTP3_ID_ERROR);
          s->has_goaway = 1;
          s->goaway_id = id;
        }
    }
  else if (frame.type == 1 && s->state != MULIB_HTTP3_AFTER_TRAILERS_)
    ++s->state;
  else if (frame.type == 1 || (frame.type == 0 && s->state != MULIB_HTTP3_AFTER_HEADERS_)
           || frame.type == 3 || frame.type == 4 || frame.type == 5
           || frame.type == 7 || frame.type == 13)
    return mulib_http3_stream_error_ (s, MULIB_HTTP3_FRAME_UNEXPECTED);
  *out = frame;
  *consumed = used;
  return 1;
}

int
mulib_http3_stream_finish (mulib_http3_stream_state_t *s, int partial)
{
  if (!s || s->error)
    return -1;
  if (s->control)
    return mulib_http3_stream_error_ (s, MULIB_HTTP3_CLOSED_CRITICAL_STREAM);
  if (partial)
    return mulib_http3_stream_error_ (s, MULIB_HTTP3_FRAME_ERROR);
  if (s->state == MULIB_HTTP3_BEFORE_HEADERS_)
    return mulib_http3_stream_error_ (s, MULIB_HTTP3_REQUEST_INCOMPLETE);
  s->state = MULIB_HTTP3_FINISHED_;
  return 1;
}

static int
mulib_http3_prefix_write_ (mulib_buffer_t *b, unsigned prefix,
                            unsigned mask, uint64_t value)
{
  unsigned char byte;
  if (value > MULIB_QUIC_MAX_INTEGER)
    return 0;
  byte = (unsigned char)(prefix | (value < mask ? (unsigned)value : mask));
  if (!mulib_buffer_append (b, &byte, 1))
    return 0;
  if (value < mask)
    return 1;
  value -= mask;
  while (value >= 128)
    {
      byte = (unsigned char)(128 | (value & 127));
      if (!mulib_buffer_append (b, &byte, 1))
        return 0;
      value >>= 7;
    }
  byte = (unsigned char)value;
  return mulib_buffer_append (b, &byte, 1);
}

static int
mulib_http3_prefix_read_ (const unsigned char *p, size_t n, size_t *at,
                           unsigned mask, uint64_t *out)
{
  uint64_t value, part;
  unsigned byte;
  if (*at == n)
    return 0;
  value = p[(*at)++] & mask;
  if (value == mask)
    for (unsigned shift = 0; ; shift += 7)
      {
        if (*at == n || shift >= 63)
          return 0;
        byte = p[(*at)++];
        part = byte & 127;
        if (part > (MULIB_QUIC_MAX_INTEGER - value) >> shift)
          return 0;
        value += part << shift;
        if (!(byte & 128))
          break;
      }
  *out = value;
  return 1;
}

int
mulib_http3_qpack_literal_write (mulib_buffer_t *out,
                                 const mulib_http3_field_t *fields,
                                 size_t count)
{
  static const unsigned char prefix[2] = { 0, 0 };
  mulib_buffer_t b = { NULL, 0, 0 };
  int ok;
  if (!out || (!fields && count))
    return 0;
  ok = mulib_buffer_append (&b, prefix, sizeof (prefix));
  for (size_t i = 0; ok && i < count; ++i)
    {
      const mulib_http3_field_t *f = &fields[i];
      ok = f->name && f->name_length && (f->value || !f->value_length)
           && (f->never_index == 0 || f->never_index == 1)
           && mulib_http3_prefix_write_ (&b, f->never_index ? 0x30 : 0x20,
                                         7, f->name_length)
           && mulib_buffer_append (&b, f->name, f->name_length)
           && mulib_http3_prefix_write_ (&b, 0, 127, f->value_length)
           && mulib_buffer_append (&b, f->value, f->value_length);
    }
  if (ok)
    ok = mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}

static int
mulib_http3_qpack_fields_ (const unsigned char *p, size_t n, size_t at,
                            mulib_http3_field_t *fields, size_t cap, size_t *count)
{
  size_t i = 0;
  while (at < n)
    {
      mulib_http3_field_t f;
      uint64_t name_length, value_length;
      if ((p[at] & 0xe8) != 0x20)
        return -4;
      f.never_index = (p[at] & 0x10) != 0;
      if (!mulib_http3_prefix_read_ (p, n, &at, 7, &name_length)
          || !name_length || name_length >= n - at)
        return -1;
      f.name = p + at;
      f.name_length = (size_t)name_length;
      at += f.name_length;
      if (p[at] & 128)
        return -4;
      if (!mulib_http3_prefix_read_ (p, n, &at, 127, &value_length)
          || value_length > n - at)
        return -1;
      f.value = p + at;
      f.value_length = (size_t)value_length;
      at += f.value_length;
      if (i == cap)
        return -2;
      if (fields)
        fields[i] = f;
      ++i;
    }
  *count = i;
  return 1;
}

int
mulib_http3_qpack_literal_read (const void *data, size_t n,
                                mulib_http3_field_t *fields,
                                size_t cap, size_t *count)
{
  const unsigned char *p = (const unsigned char *)data;
  uint64_t insert_count, delta_base;
  size_t at = 0, checked;
  unsigned sign;
  int rc;
  if (!data || !count || (!fields && cap)
      || !mulib_http3_prefix_read_ (p, n, &at, 255, &insert_count) || at == n)
    return -1;
  sign = p[at] & 128;
  if (!mulib_http3_prefix_read_ (p, n, &at, 127, &delta_base))
    return -1;
  if (insert_count)
    return -4;
  if (sign || delta_base)
    return -1;
  rc = mulib_http3_qpack_fields_ (p, n, at, NULL, cap, &checked);
  if (rc != 1)
    return rc;
  return mulib_http3_qpack_fields_ (p, n, at, fields, cap, count);
}
#endif
