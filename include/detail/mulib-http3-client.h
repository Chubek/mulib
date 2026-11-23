/* Native single-request HTTP/3 client. Include via mulib-http3.h. */
#define MULIB_HTTP3_CLIENT_IMPLEMENTED
#ifndef MULIB_IMPL_HTTP3
#error "MULIB_IMPL_HTTP3_CLIENT needs MULIB_IMPL_HTTP3 in this translation unit"
#endif
#include "../mulib-uri.h"
#include "mulib-qpack-tables.h"

enum
{
  MULIB_HTTP3_HEADER_BYTES_ = 65536, MULIB_HTTP3_URL_BYTES_ = 8192
};

typedef struct mulib_http3_response_
{
  mulib_buffer_t body;
  unsigned status;
  uint64_t content_length;
  int has_length, trailers;
} mulib_http3_response_;

static int
mulib_http3_huffman_decode_ (const unsigned char *p, size_t n, unsigned char *out,
                             size_t capacity, size_t *length)
{
  int node = 0;
  unsigned pending = 0, ones = 1;
  size_t size = 0;
  for (size_t i = 0; i < n; ++i)
    for (unsigned bit = 128; bit; bit >>= 1)
      {
        unsigned one = (p[i] & bit) != 0;
        node = mulib_http3_huffman_[node][one];
        ++pending;
        ones &= one;
        if (node < 0)
          {
            if (node == -257)
              return MULIB_QUIC_PROTOCOL;
            if (size == capacity)
              return MULIB_QUIC_LIMIT;
            out[size++] = (unsigned char)(-node - 1);
            node = 0;
            pending = 0;
            ones = 1;
          }
      }
  if (pending > 7 || !ones)
    return MULIB_QUIC_PROTOCOL;
  *length = size;
  return 1;
}

static int
mulib_http3_string_ (const unsigned char *p, size_t n, size_t *at, unsigned mask,
                     unsigned flag, unsigned char *scratch, size_t capacity,
                     const unsigned char **text, size_t *length)
{
  uint64_t size;
  int huffman, rc = 1;
  if (*at == n)
    return MULIB_QUIC_PROTOCOL;
  huffman = p[*at] & flag;
  if (!mulib_http3_prefix_read_ (p, n, at, mask, &size) || size > n - *at)
    return MULIB_QUIC_PROTOCOL;
  if (huffman)
    {
      *text = scratch;
      rc = mulib_http3_huffman_decode_ (p + *at, (size_t)size, scratch, capacity, length);
    }
  else
    {
      *text = p + *at;
      *length = (size_t)size;
    }
  *at += (size_t)size;
  return rc;
}

static int
mulib_http3_field_equal_ (const unsigned char *p, size_t n, const char *s)
{
  return n == strlen (s) && !memcmp (p, s, n);
}

static int
mulib_http3_response_field_ (mulib_http3_response_ *r, const unsigned char *name,
                             size_t nn, const unsigned char *value, size_t vn, int *regular)
{
  uint64_t number = 0;
  if (!nn || (vn && (value[0] == ' ' || value[0] == '\t'
                     || value[vn - 1] == ' ' || value[vn - 1] == '\t')))
    return 0;
  for (size_t i = 0; i < vn; ++i)
    if ((value[i] < 0x20 && value[i] != '\t') || value[i] == 0x7f)
      return 0;
  if (name[0] == ':')
    {
      if (*regular || r->status || !mulib_http3_field_equal_ (name, nn, ":status")
          || vn != 3 || value[0] < '1' || value[0] > '5'
          || value[1] < '0' || value[1] > '9' || value[2] < '0' || value[2] > '9')
        return 0;
      r->status = 100u * (value[0] - '0') + 10u * (value[1] - '0') + (value[2] - '0');
      return r->status != 101;
    }
  *regular = 1;
  for (size_t i = 0; i < nn; ++i)
    if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= '0' && name[i] <= '9')
          || (name[i] && strchr ("!#$%&'*+-.^_`|~", name[i]))))
      return 0;
  if (mulib_http3_field_equal_ (name, nn, "connection")
      || mulib_http3_field_equal_ (name, nn, "keep-alive")
      || mulib_http3_field_equal_ (name, nn, "proxy-connection")
      || mulib_http3_field_equal_ (name, nn, "transfer-encoding")
      || mulib_http3_field_equal_ (name, nn, "upgrade")
      || mulib_http3_field_equal_ (name, nn, "te"))
    return 0;
  if (mulib_http3_field_equal_ (name, nn, "content-length"))
    {
      if (!vn || r->trailers || r->has_length)
        return 0;
      for (size_t i = 0; i < vn; ++i)
        {
          unsigned digit = (unsigned)value[i] - '0';
          if (digit > 9 || number > (UINT64_MAX - digit) / 10)
            return 0;
          number = number * 10 + digit;
        }
      r->has_length = 1;
      r->content_length = number;
    }
  return 1;
}

static int
mulib_http3_response_headers_ (mulib_http3_response_ *response, const unsigned char *p, size_t n)
{
  unsigned char scratch[MULIB_HTTP3_HEADER_BYTES_];
  mulib_http3_response_ section = { { NULL, 0, 0 }, 0, 0, 0, response->status != 0 };
  size_t at = 0, remaining = sizeof scratch;
  uint64_t idx, base;
  int regular = section.trailers;
  /* With no inserts, an unsigned Delta Base can be any value. A negative
   * Delta Base would put Base below zero. */
  if (!p || !mulib_http3_prefix_read_ (p, n, &at, 255, &idx) || idx || at == n || (p[at] & 128)
      || !mulib_http3_prefix_read_ (p, n, &at, 127, &base))
    return MULIB_QUIC_PROTOCOL;
  while (at < n)
    {
      const unsigned char *name, *value = NULL;
      size_t nn, vn = 0;
      unsigned first = p[at], index_only = first & 128;
      int rc;
      if (index_only || (first & 0xc0) == 0x40)
        {
          if (!(first & (index_only ? 64 : 16))
              || !mulib_http3_prefix_read_ (p, n, &at, index_only ? 63 : 15, &idx)
              || idx >= sizeof mulib_http3_static_ / sizeof *mulib_http3_static_)
            return MULIB_QUIC_PROTOCOL;
          name = (const unsigned char *)mulib_http3_static_[idx].name;
          nn = strlen ((const char *)name);
          if (index_only)
            {
              value = (const unsigned char *)mulib_http3_static_[idx].value;
              vn = strlen ((const char *)value);
            }
        }
      else if ((first & 0xe0) == 0x20)
        {
          rc = mulib_http3_string_ (p, n, &at, 7, 8, scratch, remaining, &name, &nn);
          if (rc != 1)
            return rc;
        }
      else
        return MULIB_QUIC_PROTOCOL;
      if (remaining < 32 || nn > remaining - 32)
        return MULIB_QUIC_LIMIT;
      if (!index_only)
        {
          rc = mulib_http3_string_ (p, n, &at, 127, 128, scratch + nn,
                                    remaining - nn - 32, &value, &vn);
          if (rc != 1)
            return rc;
        }
      if (vn > remaining - nn - 32)
        return MULIB_QUIC_LIMIT;
      remaining -= nn + vn + 32;
      if (!mulib_http3_response_field_ (&section, name, nn, value, vn, &regular))
        return MULIB_QUIC_PROTOCOL;
    }
  if (response->status)
    {
      if (response->trailers)
        return MULIB_QUIC_PROTOCOL;
      response->trailers = 1;
    }
  else if (!section.status)
    return MULIB_QUIC_PROTOCOL;
  else if (section.status >= 200)
    {
      if (section.status == 204 && section.has_length)
        return MULIB_QUIC_PROTOCOL;
      response->status = section.status;
      response->has_length = section.has_length;
      response->content_length = section.content_length;
    }
  else if (section.has_length)
    return MULIB_QUIC_PROTOCOL;
  return 1;
}

static int
mulib_http3_peer_streams_ (mulib_quic_client_t *c, mulib_http3_stream_state_t *control,
                           size_t cursor[3], int type[3], unsigned *seen)
{
  for (unsigned i = 0; i < 3; ++i)
    {
      mulib_quic_stream_t stream = { 0, 0, NULL, 0, 0 };
      int rc = mulib_quic_client_stream (c, 4 * i + 3, &stream);
      if (rc != 1)
        return rc;
      if (type[i] < 0)
        {
          uint64_t id;
          rc = mulib_quic_varint_read (stream.data, stream.length, &id, cursor + i);
          if (rc != 1)
            {
              if (rc < 0 || stream.fin)
                return MULIB_QUIC_PROTOCOL;
              continue;
            }
          if (id == 1) /* Server push was not enabled. */
            return MULIB_QUIC_PROTOCOL;
          if (id == 0 || id == 2 || id == 3)
            {
              if (*seen & (1u << id))
                return MULIB_QUIC_PROTOCOL;
              *seen |= 1u << id;
              type[i] = (int)id;
            }
          else
            type[i] = 4;
        }
      if (type[i] == 4)
        continue;
      if (stream.fin)
        return MULIB_QUIC_PROTOCOL; /* Closed critical stream. */
      while (cursor[i] < stream.length)
        {
          const unsigned char *p = stream.data + cursor[i];
          size_t n = stream.length - cursor[i], used;
          if (!type[i])
            {
              mulib_http3_frame_t f;
              rc = mulib_http3_stream_next (control, p, n, MULIB_HTTP3_HEADER_BYTES_, &f, &used);
              if (!rc)
                break;
              if (rc != 1)
                return rc == -1 ? MULIB_QUIC_PROTOCOL : rc;
              if (f.type == 3)
                return MULIB_QUIC_PROTOCOL; /* Cannot cancel an unpromised push. */
              if (control->has_goaway && !control->goaway_id)
                return MULIB_QUIC_CLOSED;
            }
          else if (type[i] == 2)
            {
              uint64_t capacity;
              used = 0;
              if ((*p & 0xe0) != 0x20 || (*p & 31) != 0
                  || !mulib_http3_prefix_read_ (p, n, &used, 31, &capacity) || capacity)
                return MULIB_QUIC_PROTOCOL;
            }
          else
            return MULIB_QUIC_PROTOCOL; /* Never insert/block/reference dynamic entries. */
          cursor[i] += used;
        }
    }
  return 1;
}

static int
mulib_http3_read_response_ (mulib_quic_client_t *c, mulib_http3_response_ *r, size_t limit)
{
  mulib_http3_stream_state_t control;
  size_t cursor[3] = { 0 }, at = 0;
  int type[3] = { -1, -1, -1 };
  unsigned seen = 0;
  mulib_http3_stream_init (&control, 1, 1);
  for (;;)
    {
      mulib_quic_stream_t stream = { 0, 0, NULL, 0, 0 };
      int rc = mulib_http3_peer_streams_ (c, &control, cursor, type, &seen);
      if (rc != 1)
        return rc;
      rc = mulib_quic_client_stream (c, 0, &stream);
      if (rc != 1)
        return rc;
      while (at < stream.length)
        {
          mulib_http3_frame_t f;
          size_t used;
          rc = mulib_http3_frame_read (stream.data + at, stream.length - at, limit, &f, &used);
          if (!rc)
            break;
          if (rc != 1)
            return rc == -1 ? MULIB_QUIC_PROTOCOL : rc;
          if (f.type == 1)
            {
              if (f.length > MULIB_HTTP3_HEADER_BYTES_)
                return MULIB_QUIC_LIMIT;
              rc = mulib_http3_response_headers_ (r, f.data, f.length);
              if (rc != 1)
                return rc;
            }
          else if (f.type == 0)
            {
              if (!r->status || r->trailers || r->status == 204 || r->status == 304
                  || (r->has_length && f.length > r->content_length - r->body.length))
                return MULIB_QUIC_PROTOCOL;
              if (!mulib_buffer_append (&r->body, f.data, f.length))
                return MULIB_QUIC_NOMEM;
            }
          else if (f.type == 3 || f.type == 4 || f.type == 5 || f.type == 7 || f.type == 13)
            return MULIB_QUIC_PROTOCOL;
          at += used;
        }
      if (stream.fin)
        {
          if (at != stream.length || !r->status
              || (r->has_length && r->status != 304 && r->content_length != r->body.length))
            return MULIB_QUIC_PROTOCOL;
          if ((seen & 1) && control.state != MULIB_HTTP3_BEFORE_HEADERS_)
            return 1;
          /* The peer can deliver the response before control/SETTINGS. */
        }
      rc = mulib_quic_client_poll (c);
      if (rc != 1)
        return rc;
    }
}

int
mulib_http3_get (const char *url, const mulib_quic_client_options_t *options,
                unsigned *status, mulib_buffer_t *body)
{
  static const unsigned char control[] = {
    0, 4, 9, 1, 0, 7, 0, 6, 0x80, 0x01, 0x00, 0x00
  }; /* control, SETTINGS: capacity=0, blocked=0, max_field_section_size=65536 */
  static const unsigned char encoder = 2, decoder = 3;
  mulib_quic_client_options_t opts = { NULL, NULL, NULL, 0, 0 };
  mulib_http3_response_ result = { { NULL, 0, 0 }, 0, 0, 0, 0 };
  mulib_buffer_t target = { NULL, 0, 0 }, headers = { NULL, 0, 0 }, request = { NULL, 0, 0 };
  mulib_quic_client_t *c = NULL;
  mulib_uri_t uri;
  mulib_http3_field_t fields[4];
  mulib_http3_frame_t frame;
  size_t n, shift, hn, authority_size;
  unsigned port = 443;
  char host[254];
  int rc;
  if (!url || !status || !body || body->length > body->capacity || (!body->data && body->capacity)
      || (n = strlen (url)) > MULIB_HTTP3_URL_BYTES_ || !mulib_uri_parse (url, n, &uri)
      || uri.scheme.length != 5
      || !uri.has_authority || uri.has_userinfo || !uri.host.length)
    return MULIB_QUIC_INVALID;
  for (unsigned i = 0; i < 5; ++i)
    if (uri.scheme.data[i] != "https"[i] && uri.scheme.data[i] != "HTTPS"[i])
      return MULIB_QUIC_INVALID;
  shift = uri.host.data[0] == '[' ? 1 : 0;
  hn = uri.host.length - shift * 2;
  if (!hn || hn >= sizeof host || memchr (uri.host.data, '%', uri.host.length))
    return MULIB_QUIC_INVALID;
  memcpy (host, uri.host.data + shift, hn);
  host[hn] = 0;
  if (uri.has_port)
    {
      port = 0;
      for (size_t i = 0; i < uri.port.length; ++i)
        {
          unsigned d = (unsigned char)uri.port.data[i] - '0';
          if (d > 9 || port > (65535u - d) / 10)
            return MULIB_QUIC_INVALID;
          port = port * 10 + d;
        }
      if (!port)
        return MULIB_QUIC_INVALID;
    }
  authority_size = uri.host.length + (uri.has_port ? 1 + uri.port.length : 0);
  if (options)
    opts = *options;
  opts.alpn = "h3";
  rc = MULIB_QUIC_NOMEM;
  if (!(uri.path.length ? mulib_buffer_append (&target, uri.path.data, uri.path.length)
                        : mulib_buffer_append (&target, "/", 1))
      || (uri.has_query && (!mulib_buffer_append (&target, "?", 1)
                            || !mulib_buffer_append (&target, uri.query.data, uri.query.length))))
    goto done;
  fields[0] = (mulib_http3_field_t){ (const unsigned char *)":method", (const unsigned char *)"GET", 7, 3, 0 };
  fields[1] = (mulib_http3_field_t){ (const unsigned char *)":scheme", (const unsigned char *)"https", 7, 5, 0 };
  fields[2] = (mulib_http3_field_t){ (const unsigned char *)":authority", (const unsigned char *)uri.host.data, 10, authority_size, 0 };
  fields[3] = (mulib_http3_field_t){ (const unsigned char *)":path", target.data, 5, target.length, 0 };
  if (!mulib_http3_qpack_literal_write (&headers, fields, sizeof fields / sizeof *fields))
    goto done;
  frame = (mulib_http3_frame_t){ 1, headers.data, headers.length };
  if (!mulib_http3_frame_write (&request, &frame))
    goto done;
  if ((rc = mulib_quic_client_connect (host, port, &opts, &c)) != 1
      || (rc = mulib_quic_client_send (c, 2, control, sizeof control, 0)) != 1
      || (rc = mulib_quic_client_send (c, 6, &encoder, 1, 0)) != 1
      || (rc = mulib_quic_client_send (c, 10, &decoder, 1, 0)) != 1
      || (rc = mulib_quic_client_send (c, 0, request.data, request.length, 1)) != 1)
    goto done;
  rc = mulib_http3_read_response_ (c, &result, opts.max_stream_bytes ? opts.max_stream_bytes : 1048576);
  if (rc == 1)
    {
      mulib_buffer_free (body);
      *body = result.body;
      mulib_buffer_init (&result.body);
      *status = result.status;
    }
done:
  mulib_quic_client_close (c);
  mulib_buffer_free (&result.body);
  mulib_buffer_free (&target);
  mulib_buffer_free (&headers);
  mulib_buffer_free (&request);
  return rc;
}
