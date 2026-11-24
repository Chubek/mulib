/**
 * @file mulib-http.h
 * @brief Bounded, length-aware HTTP/1.1 parsing and serialization (no
 * sockets). Define MULIB_IMPL_HTTP once, plus URI, STRBUF, BUFFER and PARSEC
 * implementations. See the manual for the deliberately strict framing policy.
 */
#ifndef MULIB_HTTP_H
#define MULIB_HTTP_H
#include "mulib-buffer.h"
#include "mulib-parsec.h"
#include "mulib-uri.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Parser/serializer result. MORE means retain all bytes and retry.
   */
  typedef enum mulib_http_result
  {
    MULIB_HTTP_MORE = 0,
    MULIB_HTTP_OK = 1,
    MULIB_HTTP_INVALID = -1,
    MULIB_HTTP_LIMIT = -2,
    MULIB_HTTP_NOMEM = -3
  } mulib_http_result;
  /** @brief EOF completes close-delimited responses; HEAD/CONNECT describe
   * request context. */
  enum
  {
    MULIB_HTTP_EOF = 1,
    MULIB_HTTP_HEAD_RESPONSE = 2,
    MULIB_HTTP_CONNECT_RESPONSE = 4,
    /** Accept validated chunk extensions and separate trailer fields. */
    MULIB_HTTP_CHUNK_METADATA = 8
  };
  /** @brief Zero limits select 32768 header bytes, 100 headers, 16 MiB body.
   */
  typedef struct mulib_http_options
  {
    size_t max_header_bytes, max_headers, max_body_bytes;
    unsigned flags;
  } mulib_http_options_t;
  /** @brief Owned name/value pair, with surrounding value whitespace removed.
   */
  typedef struct mulib_http_header
  {
    char *name, *value;
  } mulib_http_header_t;
  /** @brief Owned parsed message. Encoder borrows the same structure. */
  typedef struct mulib_http_message
  {
    int is_response;
    char *method, *target; /* request only */
    unsigned status;
    char *reason; /* response only */
    mulib_http_header_t *headers;
    size_t header_count;
    unsigned char *body; /* decoded chunk data, binary-safe */
    size_t body_length;
    int chunked, close_delimited;
    mulib_http_header_t *trailers; /* never merged into routing/framing headers */
    size_t trailer_count;
  } mulib_http_message_t;
  /** @brief Free a parsed message; zero-initialized messages and NULL are
   * safe. */
  void mulib_http_message_free (mulib_http_message_t *message);
  /** @brief Borrow the first case-insensitive matching header, or NULL. */
  const char *mulib_http_header (const mulib_http_message_t *message,
                                 const char *name);
  /** Iterate duplicate fields (e.g. Set-Cookie). Initialize *cursor=0. */
  const char *mulib_http_header_next (const mulib_http_message_t *message,
                                      const char *name, size_t *cursor);
  /** @brief Parse one message. response is 0 for requests, 1 for responses.
   * On OK, out owns allocations and consumed identifies the exact frame
   * boundary. On failure/MORE, out is unchanged and consumed is zero. EOF
   * makes truncation invalid. out must not already own a message. consumed may
   * be NULL. HEAD/CONNECT context flags are response-only and mutually exclusive.
   */
  mulib_http_result mulib_http_parse (const void *bytes, size_t length,
                                      int response,
                                      const mulib_http_options_t *options,
                                      mulib_http_message_t *out,
                                      size_t *consumed);
  /** @brief Serialize to malloc-owned bytes (with an extra NUL not counted in
   * length). Existing framing headers must match the body. Missing framing is
   * generated; chunked selects canonical chunked output. Source fields are
   * validated before interpolation; generated wire and original start-line
   * identity are revalidated. On failure both output arguments are unchanged.
   */
  mulib_http_result mulib_http_serialize (const mulib_http_message_t *message,
                                          const mulib_http_options_t *options,
                                          unsigned char **bytes,
                                          size_t *length);
  /** Incremental response decoder. Callback receives final headers once with
   * bytes=NULL,length=0, then decoded body slices. Return 1 to continue, 0 to
   * abort (INVALID). No body retention; cumulative body and metadata are bounded.
   * Callbacks precede final framing validation; commit side effects after finish.
   * Informational responses, fixed/EOF/chunked framing and trailers supported. */
  typedef int (*mulib_http_body_fn) (void *, const mulib_http_message_t *,
                                    const void *, size_t);
  typedef struct mulib_http_stream mulib_http_stream_t;
  mulib_http_stream_t *mulib_http_stream_create (const mulib_http_options_t *,
                                                mulib_http_body_fn, void *);
  int mulib_http_stream_feed (mulib_http_stream_t *, const void *, size_t);
  /** At transport EOF, transfer owned headers/trailers (empty body) on OK.
   * Output unchanged on error. Feed/finish errors are sticky. */
  int mulib_http_stream_finish (mulib_http_stream_t *, mulib_http_message_t *);
  void mulib_http_stream_destroy (mulib_http_stream_t *);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_HTTP_H */

#if defined(MULIB_IMPL_HTTP) && !defined(MULIB_HTTP_IMPLEMENTED)
#define MULIB_HTTP_IMPLEMENTED
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C"
{
#endif
  static unsigned char
  mulib_http_lower_ (unsigned char c)
  {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
  }
  static int
  mulib_http_equal_n_ (const char *a, size_t n, const char *b, size_t m)
  {
    if (n != m)
      return 0;
    for (size_t i = 0; i < n; ++i)
      if (mulib_http_lower_ ((unsigned char)a[i])
          != mulib_http_lower_ ((unsigned char)b[i]))
        return 0;
    return 1;
  }
  static int
  mulib_http_equal_ (const char *a, const char *b)
  {
    return a && b && mulib_http_equal_n_ (a, strlen (a), b, strlen (b));
  }
  static int
  mulib_http_token_byte_ (unsigned char c, void *user)
  {
    (void)user;
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
           || (c >= '0' && c <= '9') || (c && strchr ("!#$%&'*+-.^_`|~", c));
  }
  static int
  mulib_http_value_byte_ (unsigned char c, void *user)
  {
    (void)user;
    return c == '\t' || (c >= 32 && c != 127);
  }
  static mulib_http_result
  mulib_http_run_ (const mulib_parsec_parser_t *p, const char *s, size_t n,
                   mulib_parsec_result_t *r)
  {
    mulib_parsec_options_t limits = { 32, 0, 4 };
    int rc;
    limits.max_steps = n > (SIZE_MAX - 128) / 8 ? SIZE_MAX : n * 8 + 128;
    rc = mulib_parsec_run (p, s, n, &limits, r);
    if (rc == 1)
      return MULIB_HTTP_OK;
    if (r->error.code == MULIB_PARSEC_MEMORY)
      return MULIB_HTTP_NOMEM;
    return r->error.code == MULIB_PARSEC_LIMIT ? MULIB_HTTP_LIMIT
                                               : MULIB_HTTP_INVALID;
  }
  static int
  mulib_http_token_ (const char *s, size_t n)
  {
    if (!n) return 0;
    for (size_t i = 0; i < n; ++i)
      if (!mulib_http_token_byte_ ((unsigned char)s[i], NULL)) return 0;
    return 1;
  }
  static int
  mulib_http_value_ (const char *s, size_t n)
  {
    for (size_t i = 0; i < n; ++i)
      if (!mulib_http_value_byte_ ((unsigned char)s[i], NULL)) return 0;
    return 1;
  }
  static int
  mulib_http_hex_ (const char *s, size_t n)
  {
    if (!n) return 0;
    for (size_t i = 0; i < n; ++i)
      if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')
            || (s[i] >= 'A' && s[i] <= 'F'))) return 0;
    return 1;
  }
  /* No allocations or work proportional to a caller string beyond its limit. */
  static mulib_http_result
  mulib_http_length_ (const char *s, size_t limit, size_t *n)
  {
    size_t k = 0;
    if (!s) return MULIB_HTTP_INVALID;
    while (k < limit && s[k]) ++k;
    if (k == limit && s[k]) return MULIB_HTTP_LIMIT;
    *n = k; return MULIB_HTTP_OK;
  }
  static int
  mulib_http_budget_ (size_t *used, size_t n, size_t limit)
  {
    if (*used > limit || n > limit - *used) return 0;
    *used += n; return 1;
  }
  static int
  mulib_http_flags_ (int response, unsigned flags)
  {
    unsigned context = flags & (MULIB_HTTP_HEAD_RESPONSE | MULIB_HTTP_CONNECT_RESPONSE);
    return !(flags & ~15u)
           && context != (MULIB_HTTP_HEAD_RESPONSE | MULIB_HTTP_CONNECT_RESPONSE)
           && (response || !context);
  }
  static char *
  mulib_http_copy_ (const char *s, size_t n)
  {
    char *p;
    if (n == SIZE_MAX || !(p = (char *)malloc (n + 1)))
      return NULL;
    if (n)
      memcpy (p, s, n);
    p[n] = 0;
    return p;
  }
  /* Syntax and capture boundaries are owned by the shared combinator engine;
   * framing, numeric overflow and authority policy remain HTTP semantics. */
  static mulib_http_result
  mulib_http_start_ (const char *s, size_t n, mulib_http_message_t *m)
  {
    mulib_parsec_parser_t token_byte
        = mulib_parsec_satisfy (mulib_http_token_byte_, NULL, "method");
    mulib_parsec_parser_t value_byte
        = mulib_parsec_satisfy (mulib_http_value_byte_, NULL, "reason");
    mulib_parsec_parser_t visible = mulib_parsec_range (33, 126),
                          digit = mulib_parsec_range ('0', '9');
    mulib_parsec_parser_t token
        = mulib_parsec_repeat (&token_byte, 1, SIZE_MAX),
        target = mulib_parsec_repeat (&visible, 1, SIZE_MAX);
    mulib_parsec_parser_t reason
        = mulib_parsec_repeat (&value_byte, 0, SIZE_MAX),
        parts[7], grammar;
    mulib_parsec_result_t result;
    mulib_http_result rc;
    if (m->is_response)
      {
        parts[0] = mulib_parsec_literal ("HTTP/1.1 ", 9);
        parts[1] = mulib_parsec_range ('1', '5');
        parts[2] = mulib_parsec_repeat (&digit, 2, 2);
        parts[3] = mulib_parsec_byte (' ');
        parts[4] = mulib_parsec_capture (&reason, 0);
        parts[5] = mulib_parsec_end ();
        grammar = mulib_parsec_sequence (parts, 6);
      }
    else
      {
        parts[0] = mulib_parsec_capture (&token, 0);
        parts[1] = mulib_parsec_byte (' ');
        parts[2] = mulib_parsec_capture (&target, 1);
        parts[3] = mulib_parsec_literal (" HTTP/1.1", 9);
        parts[4] = mulib_parsec_end ();
        grammar = mulib_parsec_sequence (parts, 5);
      }
    rc = mulib_http_run_ (&grammar, s, n, &result);
    if (rc == MULIB_HTTP_OK)
      {
        mulib_parsec_capture_t c = result.captures[0];
        if (m->is_response)
          {
            m->status = (unsigned)(s[9] - '0') * 100
                        + (unsigned)(s[10] - '0') * 10
                        + (unsigned)(s[11] - '0');
            m->reason = mulib_http_copy_ (s + c.offset, c.length);
            if (!m->reason)
              rc = MULIB_HTTP_NOMEM;
          }
        else
          {
            m->method = mulib_http_copy_ (s + c.offset, c.length);
            c = result.captures[1];
            m->target = mulib_http_copy_ (s + c.offset, c.length);
            if (!m->method || !m->target)
              rc = MULIB_HTTP_NOMEM;
          }
      }
    mulib_parsec_result_free (&result);
    return rc;
  }
  static mulib_http_result
  mulib_http_field_ (const char *s, size_t n, mulib_http_header_t *out)
  {
    mulib_parsec_parser_t token_byte
        = mulib_parsec_satisfy (mulib_http_token_byte_, NULL, "header name");
    mulib_parsec_parser_t value_byte
        = mulib_parsec_satisfy (mulib_http_value_byte_, NULL, "header value");
    mulib_parsec_parser_t name
        = mulib_parsec_repeat (&token_byte, 1, SIZE_MAX),
        value = mulib_parsec_repeat (&value_byte, 0, SIZE_MAX);
    mulib_parsec_parser_t parts[4], grammar;
    mulib_parsec_result_t result;
    mulib_http_result rc;
    parts[0] = mulib_parsec_capture (&name, 0);
    parts[1] = mulib_parsec_byte (':');
    parts[2] = mulib_parsec_capture (&value, 1);
    parts[3] = mulib_parsec_end ();
    grammar = mulib_parsec_sequence (parts, 4);
    rc = mulib_http_run_ (&grammar, s, n, &result);
    if (rc == MULIB_HTTP_OK)
      {
        mulib_parsec_capture_t a = result.captures[0], b = result.captures[1];
        size_t begin = b.offset, end = begin + b.length;
        while (begin < end && (s[begin] == ' ' || s[begin] == '\t'))
          ++begin;
        while (end > begin && (s[end - 1] == ' ' || s[end - 1] == '\t'))
          --end;
        out->name = mulib_http_copy_ (s + a.offset, a.length);
        out->value = mulib_http_copy_ (s + begin, end - begin);
        if (!out->name || !out->value)
          {
            free (out->name);
            free (out->value);
            rc = MULIB_HTTP_NOMEM;
          }
      }
    mulib_parsec_result_free (&result);
    return rc;
  }
  static mulib_http_options_t
  mulib_http_options_ (const mulib_http_options_t *o)
  {
    mulib_http_options_t v = { 32768, 100, 16u * 1024u * 1024u, 0 };
    if (o)
      {
        if (o->max_header_bytes)
          v.max_header_bytes = o->max_header_bytes;
        if (o->max_headers)
          v.max_headers = o->max_headers;
        if (o->max_body_bytes)
          v.max_body_bytes = o->max_body_bytes;
        v.flags = o->flags;
      }
    return v;
  }
  void
  mulib_http_message_free (mulib_http_message_t *m)
  {
    if (!m)
      return;
    free (m->method);
    free (m->target);
    free (m->reason);
    free (m->body);
    for (size_t i = 0; i < m->header_count; ++i)
      {
        free (m->headers[i].name);
        free (m->headers[i].value);
      }
    free (m->headers);
    for (size_t i = 0; i < m->trailer_count; ++i)
      { free (m->trailers[i].name); free (m->trailers[i].value); }
    free (m->trailers);
    memset (m, 0, sizeof (*m));
  }
  const char *
  mulib_http_header (const mulib_http_message_t *m, const char *name)
  {
    size_t cursor = 0;
    return mulib_http_header_next (m, name, &cursor);
  }
  const char *
  mulib_http_header_next (const mulib_http_message_t *m, const char *name, size_t *cursor)
  {
    if (!m || !name || !cursor || (!m->headers && m->header_count))
      return NULL;
    while (*cursor < m->header_count) {
      size_t i = (*cursor)++;
      if (mulib_http_equal_ (m->headers[i].name, name)) return m->headers[i].value;
    }
    return NULL;
  }
  /* RFC 9112 chunk-ext: token names, optional token/quoted-string values.
   * Metadata is validated then discarded; it cannot change body framing. */
  static int
  mulib_http_extensions_ (const char *s, size_t n)
  {
    size_t i = 0, start;
    while (i < n) {
      while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
      if (i == n || s[i++] != ';') return 0;
      while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
      start = i;
      while (i < n && mulib_http_token_byte_ ((unsigned char)s[i], NULL)) ++i;
      if (i == start) return 0;
      start = i;
      while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
      if (i < n && s[i] == '=') {
        ++i;
        while (i < n && (s[i] == ' ' || s[i] == '\t')) ++i;
        if (i < n && s[i] == '"') {
          ++i;
          while (i < n && s[i] != '"') {
            unsigned char c = (unsigned char)s[i++];
            if (c == '\\') { if (i == n) return 0; c = (unsigned char)s[i++]; }
            if (!mulib_http_value_byte_ (c, NULL)) return 0;
          }
          if (i == n) return 0;
          ++i;
        } else {
          start = i;
          while (i < n && mulib_http_token_byte_ ((unsigned char)s[i], NULL)) ++i;
          if (start == i) return 0;
        }
      } else i = start;
    }
    return 1;
  }
  static int
  mulib_http_trailer_allowed_ (const mulib_http_message_t *m, const char *name)
  {
    static const char *const forbidden[] = { "Host", "Content-Length", "Transfer-Encoding",
      "Connection", "Trailer", "TE", "Upgrade", "Authorization", "Proxy-Authorization",
      "Proxy-Authenticate", "WWW-Authenticate", "Cookie", "Set-Cookie", "Content-Type",
      "Content-Encoding", "Content-Range", "Location", "Cache-Control", "Vary",
      "Content-Location", "Content-Language", "Content-Disposition", "Range",
      "Expect", "Max-Forwards", "Age", "Expires", "Date", "Retry-After" };
    for (size_t i = 0; i < sizeof forbidden / sizeof *forbidden; ++i)
      if (mulib_http_equal_ (name, forbidden[i])) return 0;
    for (size_t i = 0; i < m->header_count; ++i) {
      const char *p = m->headers[i].value;
      if (!mulib_http_equal_ (m->headers[i].name, "Connection") || !p) continue;
      while (*p) {
        const char *start, *end;
        while (*p == ' ' || *p == '\t' || *p == ',') ++p;
        start = p;
        while (*p && *p != ',') ++p;
        end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) --end;
        if (mulib_http_equal_n_ (start, (size_t)(end - start), name, strlen (name))) return 0;
      }
    }
    return 1;
  }
  /* limit is an absolute end offset, not a limit on the whole supplied
   * pipeline. */
  static mulib_http_result
  mulib_http_line_ (const char *s, size_t n, size_t start, size_t limit,
                    size_t *end)
  {
    size_t stop = n < limit ? n : limit;
    for (size_t i = start; i < stop; ++i)
      {
        if (s[i] == '\n' || s[i] == 0)
          return MULIB_HTTP_INVALID;
        if (s[i] == '\r')
          {
            if (i + 1 >= stop)
              return n >= limit ? MULIB_HTTP_LIMIT : MULIB_HTTP_MORE;
            if (s[i + 1] != '\n')
              return MULIB_HTTP_INVALID;
            *end = i;
            return MULIB_HTTP_OK;
          }
      }
    return n >= limit ? MULIB_HTTP_LIMIT : MULIB_HTTP_MORE;
  }
  static int
  mulib_http_decimal_ (const char *s, size_t *out)
  {
    size_t v = 0;
    if (!s || !*s)
      return 0;
    for (; *s; ++s)
      {
        unsigned d = (unsigned char)*s - (unsigned)'0';
        if (d > 9 || v > (SIZE_MAX - d) / 10)
          return 0;
        v = v * 10 + d;
      }
    *out = v;
    return 1;
  }
  static int
  mulib_http_authority_ (const char *s, size_t n, mulib_uri_t *u,
                         char **storage)
  {
    mulib_strbuf_t b;
    mulib_strbuf_init (&b);
    if (!s)
      return 0;
    if (!mulib_strbuf_append (&b, "//") || !mulib_strbuf_append_n (&b, s, n))
      {
        mulib_strbuf_free (&b);
        return -1;
      }
    if (!mulib_uri_parse (b.data, b.length, u) || !u->has_authority
        || !u->host.length || u->has_userinfo || u->path.length || u->has_query
        || u->has_fragment || (u->has_port && !u->port.length))
      {
        mulib_strbuf_free (&b);
        return 0;
      }
    if (u->has_port)
      {
        size_t port = 0;
        for (size_t i = 0; i < u->port.length; ++i)
          {
            port = port * 10 + (unsigned)(u->port.data[i] - '0');
            if (port > 65535)
              {
                mulib_strbuf_free (&b);
                return 0;
              }
          }
      }
    *storage = b.data;
    return 1;
  }
  static unsigned
  mulib_http_port_ (const mulib_uri_t *u, unsigned fallback)
  {
    unsigned v = 0;
    if (!u->has_port)
      return fallback;
    for (size_t i = 0; i < u->port.length; ++i)
      {
        if (v > 65535)
          return 65536;
        v = v * 10 + (unsigned)(u->port.data[i] - '0');
      }
    return v;
  }
  static int
  mulib_http_target_ (const mulib_http_message_t *m, const char *host)
  {
    mulib_uri_t h, t;
    char *hs = NULL, *ts = NULL;
    int ok = 0, parsed;
    size_t host_start = 0, host_end = host ? strlen (host) : 0;
    /* Validate the original bytes, not just the target extracted from
     * generated wire: otherwise CRLF can terminate the line and introduce new
     * headers. */
    for (const unsigned char *p = (const unsigned char *)m->target; *p; ++p)
      if (*p <= 32 || *p >= 127)
        return 0;
    while (host_start < host_end
           && (host[host_start] == ' ' || host[host_start] == '\t'))
      ++host_start;
    while (host_end > host_start
           && (host[host_end - 1] == ' ' || host[host_end - 1] == '\t'))
      --host_end;
    parsed = mulib_http_authority_ (host ? host + host_start : NULL,
                                    host_end - host_start, &h, &hs);
    if (parsed != 1)
      return parsed;
    if (!strcmp (m->target, "*"))
      {
        ok = !strcmp (m->method, "OPTIONS");
        goto done;
      }
    if (!strcmp (m->method, "CONNECT"))
      {
        parsed
            = mulib_http_authority_ (m->target, strlen (m->target), &t, &ts);
        if (parsed != 1)
          {
            ok = parsed;
            goto done;
          }
        if (!t.has_port)
          goto done;
      }
    else
      {
        if (!mulib_uri_parse (m->target, strlen (m->target), &t)
            || t.has_fragment || t.has_userinfo)
          goto done;
        if (!t.scheme.length)
          {
            ok = m->target[0] == '/' && !t.has_authority;
            goto done;
          }
        if ((!mulib_http_equal_n_ (t.scheme.data, t.scheme.length, "http", 4)
             && !mulib_http_equal_n_ (t.scheme.data, t.scheme.length, "https",
                                      5))
            || !t.has_authority || !t.host.length
            || (t.has_port && !t.port.length))
          goto done;
      }
    {
      unsigned def = t.scheme.length == 5 ? 443 : 80;
      unsigned tp = mulib_http_port_ (&t, def),
               hp = mulib_http_port_ (&h, def);
      ok = tp <= 65535 && hp <= 65535 && tp == hp
           && mulib_http_equal_n_ (t.host.data, t.host.length, h.host.data,
                                   h.host.length);
    }
  done:
    free (hs);
    free (ts);
    return ok;
  }
  static int
  mulib_http_no_body_ (const mulib_http_message_t *m, unsigned flags)
  {
    return m->is_response
           && ((flags & MULIB_HTTP_HEAD_RESPONSE) || m->status < 200
               || m->status == 204 || m->status == 304
               || ((flags & MULIB_HTTP_CONNECT_RESPONSE) && m->status >= 200
                   && m->status < 300));
  }
  mulib_http_result
  mulib_http_parse (const void *bytes, size_t n, int response,
                    const mulib_http_options_t *options,
                    mulib_http_message_t *out, size_t *consumed)
  {
    const char *s = (const char *)bytes;
    mulib_http_message_t m;
    mulib_buffer_t body;
    mulib_http_options_t o = mulib_http_options_ (options);
    mulib_http_result r;
    size_t end, pos = 0, content_length = 0, cl_count = 0, te_count = 0,
                hosts = 0;
    const char *host = NULL;
    if (consumed)
      *consumed = 0;
    if ((!s && n) || !out || (response != 0 && response != 1)
         || !mulib_http_flags_ (response, o.flags))
      return MULIB_HTTP_INVALID;
    memset (&m, 0, sizeof (m));
    mulib_buffer_init (&body);
    m.is_response = response;
    r = mulib_http_line_ (s, n, 0, o.max_header_bytes, &end);
    if (r != MULIB_HTTP_OK)
      goto fail;
    r = mulib_http_start_ (s, end, &m);
    if (r != MULIB_HTTP_OK)
      goto fail;
    pos = end + 2;
    for (;;)
      {
        mulib_http_header_t h, *next;
        r = mulib_http_line_ (s, n, pos, o.max_header_bytes, &end);
        if (r != MULIB_HTTP_OK)
          goto fail;
        if (end == pos)
          {
            pos = end + 2;
            break;
          }
        if (m.header_count >= o.max_headers
            || m.header_count >= SIZE_MAX / sizeof (*next))
          {
            r = MULIB_HTTP_LIMIT;
            goto fail;
          }
        r = mulib_http_field_ (s + pos, end - pos, &h);
        if (r != MULIB_HTTP_OK)
          goto fail;
        next = (mulib_http_header_t *)realloc (
            m.headers, (m.header_count + 1) * sizeof (*next));
        if (!next)
          {
            free (h.name);
            free (h.value);
            r = MULIB_HTTP_NOMEM;
            goto fail;
          }
        m.headers = next;
        m.headers[m.header_count++] = h;
        if (mulib_http_equal_ (h.name, "Content-Length"))
          {
            if (++cl_count != 1
                || !mulib_http_decimal_ (h.value, &content_length))
              {
                r = MULIB_HTTP_INVALID;
                goto fail;
              }
          }
        else if (mulib_http_equal_ (h.name, "Transfer-Encoding"))
          {
            if (++te_count != 1 || !mulib_http_equal_ (h.value, "chunked"))
              {
                r = MULIB_HTTP_INVALID;
                goto fail;
              }
          }
        else if (mulib_http_equal_ (h.name, "Host"))
          {
            ++hosts;
            host = h.value;
          }
        pos = end + 2;
      }
    if ((cl_count && te_count) || hosts > 1 || (!response && hosts != 1))
      {
        r = MULIB_HTTP_INVALID;
        goto fail;
      }
    if (!response)
      {
        int valid_target = mulib_http_target_ (&m, host);
        if (valid_target != 1)
          {
            r = valid_target < 0 ? MULIB_HTTP_NOMEM : MULIB_HTTP_INVALID;
            goto fail;
          }
      }
    m.chunked = te_count != 0;
    if (response && m.status == 205 && cl_count && content_length)
      {
        r = MULIB_HTTP_INVALID;
        goto fail;
      }
    if (mulib_http_no_body_ (&m, o.flags))
      {
        if (te_count
            || ((m.status < 200 || m.status == 204
                 || ((o.flags & MULIB_HTTP_CONNECT_RESPONSE)
                     && m.status < 300))
                && cl_count))
          {
            r = MULIB_HTTP_INVALID;
            goto fail;
          }
      }
    else if (te_count)
      {
        for (;;)
          {
            size_t chunk = 0, hex_end,
                   line_limit = pos > SIZE_MAX - 128 ? SIZE_MAX : pos + 128;
            r = mulib_http_line_ (s, n, pos, line_limit, &end);
            if (r != MULIB_HTTP_OK)
              goto fail;
            hex_end = pos;
            while (hex_end < end && mulib_http_hex_ (s + hex_end, 1)) ++hex_end;
            if (hex_end == pos || (hex_end != end
                && (!(o.flags & MULIB_HTTP_CHUNK_METADATA)
                    || !mulib_http_extensions_ (s + hex_end, end - hex_end))))
              {
                r = MULIB_HTTP_INVALID;
                goto fail;
              }
            for (size_t i = pos; i < hex_end; ++i)
              {
                unsigned char c = (unsigned char)s[i];
                unsigned d;
                if (c >= '0' && c <= '9')
                  d = c - '0';
                else if (c >= 'a' && c <= 'f')
                  d = c - 'a' + 10;
                else
                  d = c - 'A' + 10;
                if (chunk > (SIZE_MAX - d) / 16)
                  {
                    r = MULIB_HTTP_LIMIT;
                    goto fail;
                  }
                chunk = chunk * 16 + d;
              }
            pos = end + 2;
            if (!chunk)
              {
                if (o.flags & MULIB_HTTP_CHUNK_METADATA) {
                  size_t trailer_limit = o.max_header_bytes > SIZE_MAX - pos
                      ? SIZE_MAX : pos + o.max_header_bytes;
                  for (;;) {
                    mulib_http_header_t h, *next;
                    r = mulib_http_line_ (s, n, pos, trailer_limit, &end);
                    if (r != MULIB_HTTP_OK) goto fail;
                    if (end == pos) { pos += 2; break; }
                    if (m.trailer_count >= o.max_headers - m.header_count
                        || m.trailer_count >= SIZE_MAX / sizeof (*next)) {
                      r = MULIB_HTTP_LIMIT; goto fail;
                    }
                    r = mulib_http_field_ (s + pos, end - pos, &h);
                    if (r != MULIB_HTTP_OK) goto fail;
                    if (!mulib_http_trailer_allowed_ (&m, h.name)) {
                      free (h.name); free (h.value); r = MULIB_HTTP_INVALID; goto fail;
                    }
                    next = (mulib_http_header_t *)realloc (m.trailers,
                        (m.trailer_count + 1) * sizeof (*next));
                    if (!next) { free (h.name); free (h.value); r = MULIB_HTTP_NOMEM; goto fail; }
                    m.trailers = next; m.trailers[m.trailer_count++] = h;
                    pos = end + 2;
                  }
                  break;
                }
                if (n - pos < 2)
                  {
                    r = MULIB_HTTP_MORE;
                    goto fail;
                  }
                if (s[pos] != '\r' || s[pos + 1] != '\n')
                  {
                    r = MULIB_HTTP_INVALID;
                    goto fail;
                  }
                pos += 2;
                break;
              }
            if (response && m.status == 205)
              {
                r = MULIB_HTTP_INVALID;
                goto fail;
              }
            if (chunk > o.max_body_bytes - body.length)
              {
                r = MULIB_HTTP_LIMIT;
                goto fail;
              }
            if (chunk > n - pos || n - pos - chunk < 2)
              {
                r = MULIB_HTTP_MORE;
                goto fail;
              }
            if (s[pos + chunk] != '\r' || s[pos + chunk + 1] != '\n')
              {
                r = MULIB_HTTP_INVALID;
                goto fail;
              }
            if (!mulib_buffer_append (&body, s + pos, chunk))
              {
                r = MULIB_HTTP_NOMEM;
                goto fail;
              }
            pos += chunk + 2;
          }
      }
    else if (cl_count)
      {
        if (content_length > o.max_body_bytes)
          {
            r = MULIB_HTTP_LIMIT;
            goto fail;
          }
        if (content_length > n - pos)
          {
            r = MULIB_HTTP_MORE;
            goto fail;
          }
        if (!mulib_buffer_append (&body, s + pos, content_length))
          {
            r = MULIB_HTTP_NOMEM;
            goto fail;
          }
        pos += content_length;
      }
    else if (response)
      {
        m.close_delimited = 1;
        if (m.status == 205 && n != pos)
          {
            r = MULIB_HTTP_INVALID;
            goto fail;
          }
        if (n - pos > o.max_body_bytes)
          {
            r = MULIB_HTTP_LIMIT;
            goto fail;
          }
        if (!(o.flags & MULIB_HTTP_EOF))
          {
            r = MULIB_HTTP_MORE;
            goto fail;
          }
        if (!mulib_buffer_append (&body, s + pos, n - pos))
          {
            r = MULIB_HTTP_NOMEM;
            goto fail;
          }
        pos = n;
      }
    m.body = body.data;
    m.body_length = body.length;
    *out = m;
    if (consumed)
      *consumed = pos;
    return MULIB_HTTP_OK;
  fail:
    mulib_http_message_free (&m);
    mulib_buffer_free (&body);
    return r == MULIB_HTTP_MORE && (o.flags & MULIB_HTTP_EOF)
               ? MULIB_HTTP_INVALID
               : r;
  }
  mulib_http_result
  mulib_http_serialize (const mulib_http_message_t *m,
                        const mulib_http_options_t *options,
                        unsigned char **bytes, size_t *length)
  {
    mulib_http_options_t o = mulib_http_options_ (options);
    mulib_strbuf_t b;
    mulib_http_message_t check;
    mulib_http_result r = MULIB_HTTP_NOMEM;
    char number[64];
    size_t used = 0, header_bytes = 0, a, z;
    size_t has_cl = 0, has_te = 0;
    int no_body;
    if (!m || !bytes || !length || (m->is_response != 0 && m->is_response != 1)
        || (!m->body && m->body_length) || (!m->headers && m->header_count)
        || (!m->trailers && m->trailer_count)
        || (m->trailer_count && (!m->chunked || !(o.flags & MULIB_HTTP_CHUNK_METADATA)))
        || !mulib_http_flags_ (m->is_response, o.flags))
      return MULIB_HTTP_INVALID;
    if (m->body_length > o.max_body_bytes || m->header_count > o.max_headers)
      return MULIB_HTTP_LIMIT;
    if (m->trailer_count > o.max_headers - m->header_count) return MULIB_HTTP_LIMIT;
    /* Preflight the entire header block before allocating the wire buffer or
     * invoking the URI parser. Individual and aggregate limits both matter. */
    no_body = mulib_http_no_body_ (m, o.flags);
    if ((no_body && (m->body_length || m->chunked))
        || (m->is_response && m->status == 205 && m->body_length))
      return MULIB_HTTP_INVALID;
    if (m->is_response)
      {
        if (m->status < 100 || m->status > 599) return MULIB_HTTP_INVALID;
        r = mulib_http_length_ (m->reason, o.max_header_bytes, &a);
        if (r != MULIB_HTTP_OK) return r;
        if (!mulib_http_value_ (m->reason, a)) return MULIB_HTTP_INVALID;
        if (!mulib_http_budget_ (&header_bytes, 15, o.max_header_bytes)
            || !mulib_http_budget_ (&header_bytes, a, o.max_header_bytes))
          return MULIB_HTTP_LIMIT;
      }
    else
      {
        r = mulib_http_length_ (m->method, o.max_header_bytes, &a);
        if (r != MULIB_HTTP_OK) return r;
        r = mulib_http_length_ (m->target, o.max_header_bytes, &z);
        if (r != MULIB_HTTP_OK) return r;
        if (!mulib_http_token_ (m->method, a)) return MULIB_HTTP_INVALID;
        if (!mulib_http_budget_ (&header_bytes, 12, o.max_header_bytes)
            || !mulib_http_budget_ (&header_bytes, a, o.max_header_bytes)
            || !mulib_http_budget_ (&header_bytes, z, o.max_header_bytes))
          return MULIB_HTTP_LIMIT;
      }
    for (size_t i = 0; i < m->header_count; ++i)
      {
        const char *name = m->headers[i].name, *value = m->headers[i].value;
        r = mulib_http_length_ (name, o.max_header_bytes, &a);
        if (r != MULIB_HTTP_OK) return r;
        r = mulib_http_length_ (value, o.max_header_bytes, &z);
        if (r != MULIB_HTTP_OK) return r;
        if (!mulib_http_token_ (name, a) || !mulib_http_value_ (value, z))
          return MULIB_HTTP_INVALID;
        if (!mulib_http_budget_ (&header_bytes, 4, o.max_header_bytes)
            || !mulib_http_budget_ (&header_bytes, a, o.max_header_bytes)
            || !mulib_http_budget_ (&header_bytes, z, o.max_header_bytes))
          return MULIB_HTTP_LIMIT;
        if (mulib_http_equal_ (name, "Content-Length")) ++has_cl;
        if (mulib_http_equal_ (name, "Transfer-Encoding")) ++has_te;
      }
    /* Check Connection-nominated trailer fields only after all source header
     * strings have been bounded and validated. */
    {
      size_t trailer_bytes = 2;
      for (size_t i = 0; i < m->trailer_count; ++i) {
        const char *name = m->trailers[i].name, *value = m->trailers[i].value;
        r = mulib_http_length_ (name, o.max_header_bytes, &a);
        if (r != MULIB_HTTP_OK) return r;
        r = mulib_http_length_ (value, o.max_header_bytes, &z);
        if (r != MULIB_HTTP_OK) return r;
        if (!mulib_http_token_ (name, a) || !mulib_http_value_ (value, z)
            || !mulib_http_trailer_allowed_ (m, name)) return MULIB_HTTP_INVALID;
        if (!mulib_http_budget_ (&trailer_bytes, 4, o.max_header_bytes)
            || !mulib_http_budget_ (&trailer_bytes, a, o.max_header_bytes)
            || !mulib_http_budget_ (&trailer_bytes, z, o.max_header_bytes)) return MULIB_HTTP_LIMIT;
      }
    }
    if (has_cl > 1 || has_te > 1 || (has_te && !m->chunked) || (has_cl && m->chunked))
      return MULIB_HTTP_INVALID;
    if (!has_cl && !has_te && !no_body)
      {
        if (m->header_count + m->trailer_count == o.max_headers) return MULIB_HTTP_LIMIT;
        if (m->chunked) a = sizeof ("Transfer-Encoding: chunked\r\n") - 1;
        else a = (size_t)snprintf (number, sizeof (number), "Content-Length: %zu\r\n", m->body_length);
        if (!mulib_http_budget_ (&header_bytes, a, o.max_header_bytes)) return MULIB_HTTP_LIMIT;
      }
    if (!mulib_http_budget_ (&header_bytes, 2, o.max_header_bytes)) return MULIB_HTTP_LIMIT;
    r = MULIB_HTTP_NOMEM;
    mulib_strbuf_init (&b);
    memset (&check, 0, sizeof (check));
    if (m->is_response)
      {
        (void)snprintf (number, sizeof (number), "HTTP/1.1 %u ", m->status);
        if (!mulib_strbuf_append (&b, number)
            || !mulib_strbuf_append (&b, m->reason))
          goto fail;
      }
    else
      {
        int valid_target;
        valid_target = mulib_http_target_ (m, mulib_http_header (m, "Host"));
        if (valid_target != 1)
          {
            r = valid_target < 0 ? MULIB_HTTP_NOMEM : MULIB_HTTP_INVALID;
            goto fail;
          }
        if (!mulib_strbuf_append (&b, m->method)
            || !mulib_strbuf_putc (&b, ' ')
            || !mulib_strbuf_append (&b, m->target)
            || !mulib_strbuf_append (&b, " HTTP/1.1"))
          goto fail;
      }
    if (!mulib_strbuf_append (&b, "\r\n"))
      goto fail;
    for (size_t i = 0; i < m->header_count; ++i)
      {
        const char *name = m->headers[i].name, *value = m->headers[i].value;
        if (!mulib_strbuf_append (&b, name) || !mulib_strbuf_append (&b, ": ")
            || !mulib_strbuf_append (&b, value)
            || !mulib_strbuf_append (&b, "\r\n"))
          goto fail;
      }
    if (!has_cl && !has_te && !no_body)
      {
        if (m->chunked)
          {
            if (!mulib_strbuf_append (&b, "Transfer-Encoding: chunked\r\n"))
              goto fail;
          }
        else
          {
            (void)snprintf (number, sizeof (number), "Content-Length: %zu\r\n",
                            m->body_length);
            if (!mulib_strbuf_append (&b, number))
              goto fail;
          }
      }
    if (!mulib_strbuf_append (&b, "\r\n"))
      goto fail;
    if (b.length > o.max_header_bytes)
      {
        r = MULIB_HTTP_LIMIT;
        goto fail;
      }
    if (!no_body && m->chunked)
      {
        if (m->body_length)
          {
            (void)snprintf (number, sizeof (number), "%zx\r\n",
                            m->body_length);
            if (!mulib_strbuf_append (&b, number)
                || !mulib_strbuf_append_n (&b, (const char *)m->body,
                                           m->body_length)
                || !mulib_strbuf_append (&b, "\r\n"))
              goto fail;
          }
        if (!mulib_strbuf_append (&b, "0\r\n"))
          goto fail;
        for (size_t i = 0; i < m->trailer_count; ++i)
          if (!mulib_strbuf_append (&b, m->trailers[i].name)
              || !mulib_strbuf_append (&b, ": ")
              || !mulib_strbuf_append (&b, m->trailers[i].value)
              || !mulib_strbuf_append (&b, "\r\n")) goto fail;
        if (!mulib_strbuf_append (&b, "\r\n")) goto fail;
      }
    else if (!no_body
             && !mulib_strbuf_append_n (&b, (const char *)m->body,
                                        m->body_length))
      goto fail;
    o.flags |= MULIB_HTTP_EOF;
    r = mulib_http_parse (b.data, b.length, m->is_response, &o, &check, &used);
    if (r != MULIB_HTTP_OK)
      goto fail;
    if (used != b.length || check.body_length != m->body_length
        || (!m->is_response
            && (strcmp (check.method, m->method)
                || strcmp (check.target, m->target)))
        || (m->is_response
            && (check.status != m->status || strcmp (check.reason, m->reason)))
        || (m->body_length && memcmp (check.body, m->body, m->body_length)))
      {
        r = MULIB_HTTP_INVALID;
        goto fail;
      }
    mulib_http_message_free (&check);
    *bytes = (unsigned char *)b.data;
    *length = b.length;
    return MULIB_HTTP_OK;
  fail:
    mulib_http_message_free (&check);
    mulib_strbuf_free (&b);
    return r;
  }
#ifdef __cplusplus
}
#endif
struct mulib_http_stream {
  mulib_http_options_t options;
  mulib_http_message_t message;
  mulib_strbuf_t line;
  mulib_http_body_fn callback;
  void *user;
  size_t metadata, total, remaining;
  unsigned informational;
  int state, error, first, cr, finished;
};
mulib_http_stream_t *mulib_http_stream_create (const mulib_http_options_t *o,
    mulib_http_body_fn callback, void *user)
{
  mulib_http_stream_t *s;
  if (!callback || (o && !mulib_http_flags_ (1, o->flags))) return NULL;
  s = (mulib_http_stream_t *)calloc (1, sizeof *s);
  if (s) { s->options = mulib_http_options_ (o); s->callback = callback;
    s->user = user; s->first = 1; s->message.is_response = 1; }
  return s;
}
static int mulib_http_stream_headers_ (mulib_http_stream_t *s)
{
  size_t cl = 0, te = 0, hosts = 0, length = 0;
  mulib_http_message_t *m = &s->message;
  for (size_t i = 0; i < m->header_count; ++i) {
    const mulib_http_header_t *h = &m->headers[i];
    if (mulib_http_equal_ (h->name, "Content-Length")) {
      if (++cl != 1 || !mulib_http_decimal_ (h->value, &length)) return -1;
    } else if (mulib_http_equal_ (h->name, "Transfer-Encoding")) {
      if (++te != 1 || !mulib_http_equal_ (h->value, "chunked")) return -1;
    } else if (mulib_http_equal_ (h->name, "Host") && ++hosts > 1) return -1;
  }
  if ((cl && te) || (m->status == 205 && length)) return -1;
  m->chunked = te != 0;
  if (mulib_http_no_body_ (m, s->options.flags)) {
    if (te || ((m->status < 200 || m->status == 204
        || ((s->options.flags & MULIB_HTTP_CONNECT_RESPONSE) && m->status < 300)) && cl)) return -1;
    s->state = 7;
  } else if (te) s->state = 3;
  else if (cl) {
    if (length > s->options.max_body_bytes) return -2;
    s->remaining = length; s->state = length ? 1 : 7;
  } else { s->state = 2; m->close_delimited = 1; }
  if (m->status < 200) {
    if (m->status == 101 || ++s->informational > 8) return -1;
    mulib_http_message_free (m); m->is_response = 1;
    s->state = 0; s->first = 1; s->metadata = 0; return 1;
  }
  return s->callback (s->user, m, NULL, 0) == 1 ? 1 : -1;
}
static int mulib_http_stream_line_ (mulib_http_stream_t *s)
{
  const char *p = s->line.data ? s->line.data : "";
  size_t n = s->line.length;
  if (!s->state || s->state == 6) {
    mulib_http_header_t h, *fields;
    size_t count;
    int rc;
    if (!s->state && s->first) {
      s->first = 0;
      return mulib_http_start_ (p, n, &s->message);
    }
    if (!n) {
      if (s->state == 6) { s->state = 7; return 1; }
      return mulib_http_stream_headers_ (s);
    }
    if (s->message.header_count + s->message.trailer_count >= s->options.max_headers) return -2;
    if (s->state == 6 && !(s->options.flags & MULIB_HTTP_CHUNK_METADATA)) return -1;
    rc = mulib_http_field_ (p, n, &h);
    if (rc != 1) return rc;
    if (s->state == 6 && !mulib_http_trailer_allowed_ (&s->message, h.name)) {
      free (h.name); free (h.value); return -1;
    }
    count = s->state == 6 ? s->message.trailer_count : s->message.header_count;
    fields = s->state == 6 ? s->message.trailers : s->message.headers;
    if (count >= SIZE_MAX / sizeof *fields) { free (h.name); free (h.value); return -2; }
    fields = (mulib_http_header_t *)realloc (fields, (count + 1) * sizeof *fields);
    if (!fields) { free (h.name); free (h.value); return -3; }
    fields[count] = h;
    if (s->state == 6) { s->message.trailers = fields; ++s->message.trailer_count; }
    else { s->message.headers = fields; ++s->message.header_count; }
    return 1;
  }
  if (s->state == 5) {
    if (n) return -1;
    s->state = 3; return 1;
  }
  if (s->state == 3) {
    size_t k = 0, chunk = 0;
    while (k < n && mulib_http_hex_ (p + k, 1)) ++k;
    if (!k || (k != n && (!(s->options.flags & MULIB_HTTP_CHUNK_METADATA)
        || !mulib_http_extensions_ (p + k, n - k)))) return -1;
    for (size_t i = 0; i < k; ++i) {
      unsigned char c = (unsigned char)p[i];
      unsigned d = c <= '9' ? c - '0' : (c <= 'F' ? c - 'A' + 10 : c - 'a' + 10);
      if (chunk > (SIZE_MAX - d) / 16) return -2;
      chunk = chunk * 16 + d;
    }
    if (chunk > s->options.max_body_bytes - s->total) return -2;
    if (chunk && s->message.status == 205) return -1;
    s->remaining = chunk; s->state = chunk ? 4 : 6;
    if (!chunk) s->metadata = 0;
    return 1;
  }
  return -1;
}
int mulib_http_stream_feed (mulib_http_stream_t *s, const void *bytes, size_t n)
{
  const unsigned char *p = (const unsigned char *)bytes;
  if (!s || (!p && n) || s->finished) return -1;
  if (s->error) return s->error;
  while (n) {
    if (s->state == 7) return s->error = -1;
    if (s->state == 1 || s->state == 2 || s->state == 4) {
      size_t take = s->state == 2 || n < s->remaining ? n : s->remaining;
      if (take > s->options.max_body_bytes - s->total) return s->error = -2;
      if (take && s->message.status == 205) return s->error = -1;
      if (s->callback (s->user, &s->message, p, take) != 1) return s->error = -1;
      s->total += take; p += take; n -= take;
      if (s->state != 2) {
        s->remaining -= take;
        if (!s->remaining) s->state = s->state == 1 ? 7 : 5;
      }
    } else {
      unsigned char c = *p++; --n;
      size_t limit = s->state == 0 || s->state == 6 ? s->options.max_header_bytes : 128;
      if ((s->state == 0 || s->state == 6) && ++s->metadata > limit) return s->error = -2;
      if (s->cr) {
        int rc;
        if (c != '\n') return s->error = -1;
        s->cr = 0; rc = mulib_http_stream_line_ (s);
        if (rc != 1) return s->error = rc;
        s->line.length = 0; if (s->line.data) s->line.data[0] = 0;
      } else if (c == '\r') s->cr = 1;
      else {
        if (c == '\n' || !c) return s->error = -1;
        if (s->line.length >= limit) return s->error = -2;
        if (!mulib_strbuf_putc (&s->line, (char)c)) return s->error = -3;
      }
    }
  }
  return 1;
}
int mulib_http_stream_finish (mulib_http_stream_t *s, mulib_http_message_t *out)
{
  if (!s || !out || s->finished) return -1;
  if (s->error) return s->error;
  if (s->state != 2 && s->state != 7) return s->error = -1;
  *out = s->message; memset (&s->message, 0, sizeof s->message); s->finished = 1;
  return 1;
}
void mulib_http_stream_destroy (mulib_http_stream_t *s)
{
  if (s) { mulib_http_message_free (&s->message); mulib_strbuf_free (&s->line); free (s); }
}
#endif /* implementation */
