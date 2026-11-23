/** @file mulib-quic.h
 * @brief RFC 9000 variable integers, STREAM and ACK frame codecs.
 * Define MULIB_IMPL_QUIC and BUFFER. These codecs operate on plaintext frame
 * payloads. Define MULIB_IMPL_QUIC_CLIENT separately for the native live client.
 * Parse: 1 OK, 0 truncated, -1 invalid. Views borrow input; failure unchanged.
 */
#ifndef MULIB_QUIC_H
#define MULIB_QUIC_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
#define MULIB_QUIC_MAX_INTEGER UINT64_C (0x3fffffffffffffff)
  int mulib_quic_varint_read (const void *data, size_t length, uint64_t *value,
                              size_t *consumed);
  int mulib_quic_varint_write (mulib_buffer_t *out, uint64_t value);
  typedef struct mulib_quic_ack_range
  {
    uint64_t smallest, largest;
  } mulib_quic_ack_range_t;
  typedef struct mulib_quic_ack
  {
    uint64_t delay, ect0, ect1, ce;
    size_t range_count;
    int ecn;
  } mulib_quic_ack_t;
  /** Parse ACK/ACK_ECN into descending inclusive ranges. Returns -2 when
   * range_capacity is too small. All outputs are unchanged on failure. */
  int mulib_quic_ack_read (const void *data, size_t length,
                           mulib_quic_ack_t *out, mulib_quic_ack_range_t *ranges,
                           size_t range_capacity, size_t *consumed);
  /** Append ACK/ACK_ECN with descending, disjoint and nonadjacent ranges. */
  int mulib_quic_ack_write (mulib_buffer_t *out, const mulib_quic_ack_t *ack,
                            const mulib_quic_ack_range_t *ranges);
  typedef struct mulib_quic_stream
  {
    uint64_t stream_id, offset;
    const unsigned char *data;
    size_t length;
    int fin;
  } mulib_quic_stream_t;
  int mulib_quic_stream_read (const void *data, size_t length,
                              mulib_quic_stream_t *out, size_t *consumed);
  /** Like stream_read, but also accepts implicit-length STREAM frames.
   * Supply only the remaining plaintext packet payload as input. */
  int mulib_quic_packet_stream_read (const void *data, size_t length,
                                     mulib_quic_stream_t *out, size_t *consumed);
  int mulib_quic_stream_write (mulib_buffer_t *out,
                                const mulib_quic_stream_t *stream);

  struct mulib_x509_store;
  typedef struct mulib_quic_client mulib_quic_client_t;
  typedef struct mulib_quic_client_options
  {
    const struct mulib_x509_store *trust; /* Overrides ca_file. */
    const char *ca_file; /* NULL selects system trust. */
    const char *alpn; /* NULL selects "h3". */
    unsigned timeout_ms; /* Absolute I/O budget; 0 selects 10000. */
    size_t max_stream_bytes; /* Per direction on stream 0; 0 selects 1 MiB. */
  } mulib_quic_client_options_t;
  enum
  {
    MULIB_QUIC_OK = 1, MULIB_QUIC_TIMEOUT = 0, MULIB_QUIC_INVALID = -1,
    MULIB_QUIC_LIMIT = -2, MULIB_QUIC_NOMEM = -3, MULIB_QUIC_IO = -4,
    MULIB_QUIC_AUTH = -5, MULIB_QUIC_PROTOCOL = -6, MULIB_QUIC_CLOSED = -7
  };
  /** Open a QUIC v1 connection over UDP and confirm an authenticated TLS 1.3
   * handshake. *out must be NULL; unchanged on failure. DNS uses system policy.
   * This bounded client offers one bidirectional stream and three unidirectional
   * streams per endpoint. See the manual for the supported transport profile. */
  int mulib_quic_client_connect (const char *host, unsigned port,
                                 const mulib_quic_client_options_t *options,
                                 mulib_quic_client_t **out);
  /** Send ordered bytes on stream 0, 2, 6 or 10; waits for acknowledgment.
   * Acknowledged prefixes cannot be undone if the operation fails. */
  int mulib_quic_client_send (mulib_quic_client_t *client, uint64_t stream_id,
                              const void *data, size_t length, int fin);
  /** Receive and process one UDP datagram, waiting up to the remaining budget. */
  int mulib_quic_client_poll (mulib_quic_client_t *client);
  /** View all contiguous received bytes on stream 0, 3, 7 or 11.
   * View expires at the next send, poll or close. FIN means the final size has
   * been reached. Absent streams are represented by NULL data, zero length/FIN. */
  int mulib_quic_client_stream (const mulib_quic_client_t *client,
                                uint64_t stream_id, mulib_quic_stream_t *out);
  /** Best-effort CONNECTION_CLOSE followed by releasing all owned state. */
  void mulib_quic_client_close (mulib_quic_client_t *client);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_QUIC) && !defined(MULIB_QUIC_IMPLEMENTED)
#define MULIB_QUIC_IMPLEMENTED
int
mulib_quic_varint_read (const void *p, size_t n, uint64_t *out, size_t *used)
{
  const unsigned char *s = (const unsigned char *)p;
  uint64_t v;
  size_t k;
  if ((!p && n) || !out || !used)
    return -1;
  if (!n)
    return 0;
  k = (size_t)1 << (s[0] >> 6);
  if (n < k)
    return 0;
  v = s[0] & 63;
  for (size_t i = 1; i < k; ++i)
    v = (v << 8) | s[i];
  *out = v;
  *used = k;
  return 1;
}
int
mulib_quic_varint_write (mulib_buffer_t *out, uint64_t v)
{
  unsigned char bytes[8];
  size_t n = v < 64 ? 1 : v < 16384 ? 2 : v < UINT64_C (1073741824) ? 4 : 8;
  if (v > MULIB_QUIC_MAX_INTEGER)
    return 0;
  for (size_t i = n; i; --i)
    {
      bytes[i - 1] = (unsigned char)v;
      v >>= 8;
    }
  bytes[0] |= (unsigned char)(n == 1 ? 0 : n == 2 ? 64 : n == 4 ? 128 : 192);
  return mulib_buffer_append (out, bytes, n);
}
static int
mulib_quic_stream_read_ (const void *p, size_t n, mulib_quic_stream_t *out,
                         size_t *used, int implicit_length)
{
  const unsigned char *s = (const unsigned char *)p;
  mulib_quic_stream_t f;
  uint64_t type, len;
  size_t at, k;
  int rc;
  if ((!p && n) || !out || !used)
    return -1;
  rc = mulib_quic_varint_read (p, n, &type, &at);
  if (rc != 1)
    return rc;
  if (type < 8 || type > 15 || (!(type & 2) && !implicit_length))
    return -1;
  rc = mulib_quic_varint_read (s + at, n - at, &f.stream_id, &k);
  if (rc != 1)
    return rc;
  at += k;
  f.offset = 0;
  if (type & 4)
    {
      rc = mulib_quic_varint_read (s + at, n - at, &f.offset, &k);
      if (rc != 1)
        return rc;
      at += k;
    }
  len = n - at;
  if (type & 2)
    {
      rc = mulib_quic_varint_read (s + at, n - at, &len, &k);
      if (rc != 1)
        return rc;
      at += k;
    }
  if (len > MULIB_QUIC_MAX_INTEGER - f.offset)
    return -1;
  if (len > n - at)
    return 0;
  f.data = s + at;
  f.length = (size_t)len;
  f.fin = (int)(type & 1);
  *out = f;
  *used = at + f.length;
  return 1;
}
int
mulib_quic_stream_read (const void *p, size_t n, mulib_quic_stream_t *out,
                        size_t *used)
{
  return mulib_quic_stream_read_ (p, n, out, used, 0);
}
int
mulib_quic_packet_stream_read (const void *p, size_t n,
                               mulib_quic_stream_t *out, size_t *used)
{
  return mulib_quic_stream_read_ (p, n, out, used, 1);
}
int
mulib_quic_stream_write (mulib_buffer_t *out, const mulib_quic_stream_t *f)
{
  mulib_buffer_t b = { 0 };
  int ok;
  if (!out || !f || (!f->data && f->length)
      || f->offset > MULIB_QUIC_MAX_INTEGER
      || f->length > MULIB_QUIC_MAX_INTEGER - f->offset
      || (f->fin != 0 && f->fin != 1))
    return 0;
  ok = mulib_quic_varint_write (&b, (uint64_t)(14 | f->fin))
       && mulib_quic_varint_write (&b, f->stream_id)
       && mulib_quic_varint_write (&b, f->offset)
       && mulib_quic_varint_write (&b, f->length)
       && mulib_buffer_append (&b, f->data, f->length)
       && mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
static int
mulib_quic_next_integer_ (const unsigned char *p, size_t n, size_t *at,
                          uint64_t *out)
{
  size_t used;
  int rc = mulib_quic_varint_read (p + *at, n - *at, out, &used);
  if (rc == 1)
    *at += used;
  return rc;
}

static int
mulib_quic_ack_ranges_ (const unsigned char *p, size_t n, size_t *at,
                        size_t count, uint64_t largest,
                        mulib_quic_ack_range_t *ranges)
{
  for (size_t i = 0; i < count; ++i)
    {
      uint64_t gap, span;
      int rc;
      if (i)
        {
          rc = mulib_quic_next_integer_ (p, n, at, &gap);
          if (rc != 1)
            return rc;
          if (largest < 2 || gap > largest - 2)
            return -1;
          largest -= gap + 2;
        }
      rc = mulib_quic_next_integer_ (p, n, at, &span);
      if (rc != 1)
        return rc;
      if (span > largest)
        return -1;
      if (ranges)
        {
          ranges[i].largest = largest;
          ranges[i].smallest = largest - span;
        }
      largest -= span;
    }
  return 1;
}

int
mulib_quic_ack_read (const void *data, size_t n, mulib_quic_ack_t *out,
                     mulib_quic_ack_range_t *ranges, size_t capacity,
                     size_t *used)
{
  const unsigned char *p = (const unsigned char *)data;
  mulib_quic_ack_t ack = { 0, 0, 0, 0, 0, 0 };
  uint64_t type, largest, count;
  size_t at = 0, range_start;
  int rc;
  if ((!data && n) || !out || !used || (!ranges && capacity))
    return -1;
  if (!n)
    return 0;
  rc = mulib_quic_next_integer_ (p, n, &at, &type);
  if (rc != 1)
    return rc;
  if (type != 2 && type != 3)
    return -1;
  ack.ecn = type == 3;
  if ((rc = mulib_quic_next_integer_ (p, n, &at, &largest)) != 1
      || (rc = mulib_quic_next_integer_ (p, n, &at, &ack.delay)) != 1
      || (rc = mulib_quic_next_integer_ (p, n, &at, &count)) != 1)
    return rc;
  if (count >= capacity)
    return -2;
  ack.range_count = (size_t)count + 1;
  range_start = at;
  rc = mulib_quic_ack_ranges_ (p, n, &at, ack.range_count, largest, NULL);
  if (rc != 1)
    return rc;
  if (ack.ecn
      && ((rc = mulib_quic_next_integer_ (p, n, &at, &ack.ect0)) != 1
          || (rc = mulib_quic_next_integer_ (p, n, &at, &ack.ect1)) != 1
          || (rc = mulib_quic_next_integer_ (p, n, &at, &ack.ce)) != 1))
    return rc;
  (void)mulib_quic_ack_ranges_ (p, n, &range_start, ack.range_count,
                               largest, ranges);
  *out = ack;
  *used = at;
  return 1;
}

int
mulib_quic_ack_write (mulib_buffer_t *out, const mulib_quic_ack_t *ack,
                      const mulib_quic_ack_range_t *ranges)
{
  mulib_buffer_t b = { NULL, 0, 0 };
  int ok;
  if (!out || !ack || !ranges || !ack->range_count
      || (ack->ecn != 0 && ack->ecn != 1))
    return 0;
  ok = mulib_quic_varint_write (&b, (uint64_t)(2 + ack->ecn))
       && mulib_quic_varint_write (&b, ranges[0].largest)
       && mulib_quic_varint_write (&b, ack->delay)
       && mulib_quic_varint_write (&b, ack->range_count - 1);
  for (size_t i = 0; ok && i < ack->range_count; ++i)
    {
      if (ranges[i].smallest > ranges[i].largest)
        {
          ok = 0;
          break;
        }
      if (i)
        {
          uint64_t previous = ranges[i - 1].smallest;
          ok = previous >= 2 && ranges[i].largest <= previous - 2
               && mulib_quic_varint_write (&b, previous - ranges[i].largest - 2);
        }
      if (ok)
        ok = mulib_quic_varint_write (&b, ranges[i].largest - ranges[i].smallest);
    }
  if (ok && ack->ecn)
    ok = mulib_quic_varint_write (&b, ack->ect0)
         && mulib_quic_varint_write (&b, ack->ect1)
         && mulib_quic_varint_write (&b, ack->ce);
  if (ok)
    ok = mulib_buffer_append (out, b.data, b.length);
  mulib_buffer_free (&b);
  return ok;
}
#endif
#if defined(MULIB_IMPL_QUIC_CLIENT) && !defined(MULIB_QUIC_CLIENT_IMPLEMENTED)
#include "detail/mulib-quic-client.h"
#endif
