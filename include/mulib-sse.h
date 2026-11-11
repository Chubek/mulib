/** @file mulib-sse.h
 * @brief Bounded incremental server-sent events. Define MULIB_IMPL_SSE.
 * Dependencies: STRBUF and UTF8. */
#ifndef MULIB_SSE_H
#define MULIB_SSE_H
#include "mulib-strbuf.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_sse_event {
  const char *event, *id, *data; /* borrowed until callback returns */
  size_t data_length;
} mulib_sse_event_t;
typedef int (*mulib_sse_event_fn) (void *, const mulib_sse_event_t *);
typedef struct mulib_sse_parser mulib_sse_parser_t;
/** Zero limit selects 1 MiB per event/line. Return NULL on invalid callback/OOM. */
mulib_sse_parser_t *mulib_sse_create (size_t limit, mulib_sse_event_fn, void *);
/** 1 success, -1 invalid UTF-8/input, -2 limit, -3 allocation, -7 callback stop.
 * Errors are sticky. CR, LF, CRLF, BOM, comments, multiline data and id supported.
 * finish discards an unterminated event, as required by EventSource. */
int mulib_sse_feed (mulib_sse_parser_t *, const void *, size_t);
int mulib_sse_finish (mulib_sse_parser_t *);
void mulib_sse_destroy (mulib_sse_parser_t *);
/** Encode one event to malloc-owned UTF-8; no CR/LF in event/id; no embedded NUL.
 * Multiline data is split into data fields. Outputs unchanged on failure. */
int mulib_sse_encode (const mulib_sse_event_t *, size_t limit, char **, size_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SSE) && !defined(MULIB_SSE_IMPLEMENTED)
#define MULIB_SSE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
struct mulib_sse_parser {
  mulib_strbuf_t line, data, event, id;
  size_t limit, event_bytes;
  mulib_sse_event_fn callback;
  void *user;
  int error, cr, first, has_data, finished;
};
static void mulib_sse_clear_ (mulib_strbuf_t *b)
{ b->length = 0; if (b->data) b->data[0] = 0; }
mulib_sse_parser_t *mulib_sse_create (size_t limit, mulib_sse_event_fn cb, void *u)
{
  mulib_sse_parser_t *s;
  if (!cb) return NULL;
  s = (mulib_sse_parser_t *)calloc (1, sizeof *s);
  if (s) { s->limit = limit ? limit : 1048576; s->callback = cb; s->user = u; s->first = 1; }
  return s;
}
static int mulib_sse_line_ (mulib_sse_parser_t *s)
{
  const char *p = s->line.data ? s->line.data : "", *colon, *v;
  size_t n = s->line.length, k, z;
  if (s->first) {
    s->first = 0;
    if (n >= 3 && !memcmp (p, "\xef\xbb\xbf", 3)) { p += 3; n -= 3; }
  }
  if (!mulib_utf8_validate ((const unsigned char *)p, n)) return -1;
  if (!n) {
    if (s->has_data) {
      mulib_sse_event_t e;
      e.event = s->event.length ? s->event.data : "message";
      e.id = s->id.data ? s->id.data : "";
      e.data = s->data.data; e.data_length = s->data.length - 1;
      s->data.data[e.data_length] = 0;
      if (s->callback (s->user, &e) != 1) return -7;
    }
    s->has_data = 0; s->event_bytes = 0;
    mulib_sse_clear_ (&s->data); mulib_sse_clear_ (&s->event);
    return 1;
  }
  if (p[0] == ':') return 1;
  if (n > s->limit - s->event_bytes) return -2;
  s->event_bytes += n;
  colon = (const char *)memchr (p, ':', n); k = colon ? (size_t)(colon - p) : n;
  v = colon ? colon + 1 : p + n; z = n - (size_t)(v - p);
  if (z && *v == ' ') { ++v; --z; }
  if (k == 4 && !memcmp (p, "data", 4)) {
    if (z >= s->limit - s->data.length) return -2;
    if (!mulib_strbuf_append_n (&s->data, v, z) || !mulib_strbuf_putc (&s->data, '\n')) return -3;
    s->has_data = 1;
  } else if (k == 5 && !memcmp (p, "event", 5)) {
    if (memchr (v, 0, z)) return -1;
    mulib_sse_clear_ (&s->event);
    if (!mulib_strbuf_append_n (&s->event, v, z)) return -3;
  } else if (k == 2 && !memcmp (p, "id", 2) && !memchr (v, 0, z)) {
    mulib_sse_clear_ (&s->id);
    if (!mulib_strbuf_append_n (&s->id, v, z)) return -3;
  }
  return 1;
}
int mulib_sse_feed (mulib_sse_parser_t *s, const void *bytes, size_t n)
{
  const unsigned char *p = (const unsigned char *)bytes;
  if (!s || (!p && n) || s->finished) return -1;
  if (s->error) return s->error;
  for (size_t i = 0; i < n; ++i) {
    unsigned char c = p[i];
    if (s->cr) { s->cr = 0; if (c == '\n') continue; }
    if (c == '\r' || c == '\n') {
      int rc = mulib_sse_line_ (s);
      if (rc != 1) return s->error = rc;
      mulib_sse_clear_ (&s->line); s->cr = c == '\r';
    } else {
      if (s->line.length == s->limit) return s->error = -2;
      if (!mulib_strbuf_putc (&s->line, (char)c)) return s->error = -3;
    }
  }
  return 1;
}
int mulib_sse_finish (mulib_sse_parser_t *s)
{
  if (!s) return -1;
  s->finished = 1;
  if (s->error) return s->error;
  if (!mulib_utf8_validate ((const unsigned char *)s->line.data, s->line.length)) return s->error = -1;
  return 1;
}
void mulib_sse_destroy (mulib_sse_parser_t *s)
{
  if (s) { mulib_strbuf_free (&s->line); mulib_strbuf_free (&s->data);
    mulib_strbuf_free (&s->event); mulib_strbuf_free (&s->id); free (s); }
}
int mulib_sse_encode (const mulib_sse_event_t *e, size_t limit, char **out, size_t *length)
{
  mulib_strbuf_t b = {0};
  const char *fields[2];
  int rc = -3;
  if (!e || !out || !length || (!e->data && e->data_length)) return -1;
  if (!limit) limit = 1048576;
  if (e->data_length > limit) return -2;
  if (!mulib_utf8_validate ((const unsigned char *)e->data, e->data_length)
      || (e->data_length && memchr (e->data, 0, e->data_length))) return -1;
  fields[0] = e->event; fields[1] = e->id;
  for (unsigned i = 0; i < 2; ++i) if (fields[i]) {
    size_t n = strlen (fields[i]);
    if (strpbrk (fields[i], "\r\n") || !mulib_utf8_validate ((const unsigned char *)fields[i], n)) { rc = -1; goto done; }
    if (n > limit || b.length > limit - n || limit - n - b.length < 8) { rc = -2; goto done; }
    if (!mulib_strbuf_append (&b, i ? "id: " : "event: ")
        || !mulib_strbuf_append (&b, fields[i]) || !mulib_strbuf_putc (&b, '\n')) goto done;
  }
  for (size_t pos = 0;;) {
    size_t end = pos;
    while (end < e->data_length && e->data[end] != '\r' && e->data[end] != '\n') ++end;
    if (end - pos > limit - b.length || limit - b.length - (end - pos) < 8) { rc = -2; goto done; }
    if (!mulib_strbuf_append (&b, "data: ")
        || !mulib_strbuf_append_n (&b, e->data ? e->data + pos : "", end - pos)
        || !mulib_strbuf_putc (&b, '\n')) goto done;
    if (end == e->data_length) break;
    pos = end + 1;
    if (e->data[end] == '\r' && pos < e->data_length && e->data[pos] == '\n') ++pos;
  }
  if (!mulib_strbuf_putc (&b, '\n')) goto done;
  *out = b.data; *length = b.length; return 1;
done:
  mulib_strbuf_free (&b); return rc;
}
#endif
