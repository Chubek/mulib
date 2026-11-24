/** @file mulib-openai-server.h
 * @brief Transport-independent OpenAI REST server dispatch and HTTP/SSE output.
 * Define MULIB_IMPL_OPENAI_SERVER. Dependencies: OPENAI, SSE, MULTIPART, HTTP. */
#ifndef MULIB_OPENAI_SERVER_H
#define MULIB_OPENAI_SERVER_H
#include "mulib-openai.h"
#include "mulib-sse.h"
#include "mulib-multipart.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_openai_server_request {
  const mulib_openai_operation_t *operation;
  const mulib_http_message_t *http;
  const mulib_json_t *json;
  const char *target; /* relative to base_path, borrowed */
} mulib_openai_server_request_t;
typedef struct mulib_openai_writer mulib_openai_writer_t;
/** Sink must consume all bytes synchronously and return 1, or 0 on failure.
 * Use a short-write loop when adapting sockets. All pointers are borrowed. */
typedef int (*mulib_openai_write_fn) (void *, const void *, size_t);
typedef int (*mulib_openai_handler_fn) (void *, const mulib_openai_server_request_t *, mulib_openai_writer_t *);
typedef int (*mulib_openai_authorize_fn) (void *, const mulib_openai_server_request_t *);
typedef struct mulib_openai_server {
  const char *base_path; /* NULL=/v1; empty for root mount; no trailing slash */
  const char *api_key; /* optional exact Bearer credential, borrowed */
  mulib_http_options_t limits;
  size_t max_event_bytes;
  mulib_openai_authorize_fn authorize;
  mulib_openai_handler_fn handler;
  void *user;
} mulib_openai_server_t;
mulib_openai_server_t mulib_openai_server_default (mulib_openai_handler_fn, void *);
/** Dispatch a parsed, borrowed HTTP request. Automatically emits OpenAI JSON
 * errors for 400/401/403/404/405/413/415/501 and unhandled handler failure (500).
 * Return 1 means a complete response was sent, including HTTP error responses.
 * Other results require closing the transport; bytes may already be emitted. */
int mulib_openai_server_dispatch (const mulib_openai_server_t *, const mulib_http_message_t *,
    mulib_openai_write_fn, void *);
/** Parse exactly one HTTP frame and dispatch. MORE=0: retain bytes and retry;
 * consumed=0 on parsing errors/MORE. On parse success consumed identifies the
 * frame even if dispatch fails. No callbacks before a complete valid request. */
int mulib_openai_server_process (const mulib_openai_server_t *, const void *, size_t,
    size_t *consumed, mulib_openai_write_fn, void *);
/** Complete buffered binary or JSON response. Extra headers are borrowed.
 * Framing, Connection and Content-Type are managed by the writer. */
int mulib_openai_server_reply (mulib_openai_writer_t *, unsigned status, const char *content_type,
    const mulib_http_header_t *, size_t count, const void *, size_t);
int mulib_openai_server_json (mulib_openai_writer_t *, unsigned status, const mulib_json_t *);
int mulib_openai_server_error (mulib_openai_writer_t *, unsigned status,
    const char *message, const char *type, const char *param, const char *code);
/** Begin a chunked response, emit arbitrary body chunks, finish it. SSE helpers
 * use text/event-stream and encode multiline data safely. Each write is live. */
int mulib_openai_server_begin (mulib_openai_writer_t *, unsigned status, const char *content_type,
    const mulib_http_header_t *, size_t count);
int mulib_openai_server_chunk (mulib_openai_writer_t *, const void *, size_t);
int mulib_openai_server_event (mulib_openai_writer_t *, const char *event,
                              const char *id, const mulib_json_t *data);
int mulib_openai_server_done (mulib_openai_writer_t *, int send_done_marker);
/** Decode one multipart part from request, using its Content-Type boundary. */
int mulib_openai_server_part (const mulib_openai_server_request_t *, size_t *cursor,
                              mulib_multipart_part_t *);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OPENAI_SERVER) && !defined(MULIB_OPENAI_SERVER_IMPLEMENTED)
#define MULIB_OPENAI_SERVER_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct mulib_openai_writer {
  mulib_openai_write_fn write;
  void *user;
  mulib_http_options_t limits;
  size_t max_event_bytes, written;
  int state, error, sse;
};
mulib_openai_server_t mulib_openai_server_default (mulib_openai_handler_fn handler, void *user)
{
  mulib_openai_server_t s;
  memset (&s, 0, sizeof s); s.base_path = "/v1"; s.handler = handler; s.user = user;
  s.limits.flags = MULIB_HTTP_CHUNK_METADATA; s.max_event_bytes = 1048576; return s;
}
static int mulib_openai_server_equal_ (const char *a, const char *b)
{
  if (!a || !b) return 0;
  while (*a && *b) {
    unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
    if (x >= 'A' && x <= 'Z') x += 32;
    if (y >= 'A' && y <= 'Z') y += 32;
    if (x != y) return 0;
  }
  return *a == *b;
}
static int mulib_openai_server_send_ (mulib_openai_writer_t *w, const void *p, size_t n)
{
  if (w->error) return w->error;
  if (n && w->write (w->user, p, n) != 1) return w->error = -4;
  return 1;
}
static int mulib_openai_server_reply_ (mulib_openai_writer_t *w, unsigned status, const char *type,
    const mulib_http_header_t *extra, size_t count, const void *body, size_t n, int stream)
{
  mulib_http_message_t m = {0};
  mulib_http_header_t *headers;
  unsigned char *wire = NULL;
  size_t length;
  int rc;
  if (!w || w->state || w->error || !type || (!extra && count) || (!body && n)
      || status < 200 || status > 599 || (stream && (status == 204 || status == 205 || status == 304))) return -1;
  if (count > 1000) return -2;
  headers = (mulib_http_header_t *)calloc (count + 2, sizeof *headers);
  if (!headers) return -3;
  for (size_t i = 0; i < count; ++i) {
    const char *key = extra[i].name;
    if (!key || !extra[i].value || mulib_openai_server_equal_ (key, "Content-Type")
        || mulib_openai_server_equal_ (key, "Content-Length") || mulib_openai_server_equal_ (key, "Transfer-Encoding")
        || mulib_openai_server_equal_ (key, "Connection")) { free (headers); return -1; }
    headers[i] = extra[i];
  }
  headers[count].name = (char *)"Content-Type"; headers[count].value = (char *)type;
  headers[count + 1].name = (char *)"Connection"; headers[count + 1].value = (char *)"close";
  m.is_response = 1; m.status = status; m.reason = (char *)"OpenAI";
  m.headers = headers; m.header_count = count + 2; m.body = (unsigned char *)body;
  m.body_length = n; m.chunked = stream;
  rc = mulib_http_serialize (&m, &w->limits, &wire, &length); free (headers);
  if (rc != 1) return rc;
  w->state = stream ? 1 : 2; w->sse = mulib_openai_media_type (type, "text/event-stream");
  w->written = n;
  rc = mulib_openai_server_send_ (w, wire, stream ? length - 5 : length);
  free (wire); return rc;
}
int mulib_openai_server_reply (mulib_openai_writer_t *w, unsigned status, const char *type,
    const mulib_http_header_t *headers, size_t count, const void *body, size_t n)
{ return mulib_openai_server_reply_ (w, status, type, headers, count, body, n, 0); }
int mulib_openai_server_json (mulib_openai_writer_t *w, unsigned status, const mulib_json_t *json)
{
  char *text;
  int rc;
  if (!json) return -1;
  text = mulib_json_stringify (json, 0); if (!text) return -3;
  rc = mulib_openai_server_reply (w, status, "application/json", NULL, 0, text, strlen (text));
  free (text); return rc;
}
int mulib_openai_server_error (mulib_openai_writer_t *w, unsigned status,
    const char *message, const char *type, const char *param, const char *code)
{
  mulib_json_t *e = mulib_openai_error (message, type, param, code);
  int rc;
  if (!e) return -3;
  if (status == 401) {
    mulib_http_header_t challenge = {(char *)"WWW-Authenticate", (char *)"Bearer"};
    char *text = mulib_json_stringify (e, 0);
    rc = text ? mulib_openai_server_reply (w, status, "application/json", &challenge, 1, text, strlen (text)) : -3;
    free (text);
  } else rc = mulib_openai_server_json (w, status, e);
  mulib_json_destroy (e); return rc;
}
int mulib_openai_server_begin (mulib_openai_writer_t *w, unsigned status, const char *type,
    const mulib_http_header_t *headers, size_t count)
{ return mulib_openai_server_reply_ (w, status, type, headers, count, NULL, 0, 1); }
int mulib_openai_server_chunk (mulib_openai_writer_t *w, const void *bytes, size_t n)
{
  char prefix[2 * sizeof (size_t) + 4];
  size_t limit;
  int k, rc;
  if (!w || w->state != 1 || (!bytes && n)) return -1;
  if (w->error) return w->error;
  limit = w->limits.max_body_bytes ? w->limits.max_body_bytes : 16u * 1024u * 1024u;
  if (n > limit - w->written) return w->error = -2;
  if (!n) return 1;
  k = snprintf (prefix, sizeof prefix, "%zx\r\n", n);
  if (k < 0 || (size_t)k >= sizeof prefix) return w->error = -1;
  rc = mulib_openai_server_send_ (w, prefix, (size_t)k);
  if (rc == 1) rc = mulib_openai_server_send_ (w, bytes, n);
  if (rc == 1) rc = mulib_openai_server_send_ (w, "\r\n", 2);
  if (rc == 1) w->written += n;
  return rc;
}
int mulib_openai_server_event (mulib_openai_writer_t *w, const char *event,
    const char *id, const mulib_json_t *json)
{
  mulib_sse_event_t e;
  char *text, *wire = NULL;
  size_t n;
  int rc;
  if (!w || w->state != 1 || !w->sse || !json) return -1;
  text = mulib_json_stringify (json, 0); if (!text) return -3;
  e.event = event; e.id = id; e.data = text; e.data_length = strlen (text);
  rc = mulib_sse_encode (&e, w->max_event_bytes, &wire, &n); free (text);
  if (rc == 1) rc = mulib_openai_server_chunk (w, wire, n);
  free (wire); return rc;
}
int mulib_openai_server_done (mulib_openai_writer_t *w, int marker)
{
  int rc;
  if (!w || w->state != 1 || (marker != 0 && marker != 1) || (marker && !w->sse)) return -1;
  if (marker) { rc = mulib_openai_server_chunk (w, "data: [DONE]\n\n", 14); if (rc != 1) return rc; }
  rc = mulib_openai_server_send_ (w, "0\r\n\r\n", 5);
  if (rc == 1) w->state = 2;
  return rc;
}
int mulib_openai_server_part (const mulib_openai_server_request_t *q, size_t *cursor,
    mulib_multipart_part_t *part)
{
  const char *type, *p;
  char boundary[71];
  int found = 0;
  if (!q || !q->http || !cursor || !part) return -1;
  type = mulib_http_header (q->http, "Content-Type");
  if (!mulib_openai_media_type (type, "multipart/form-data")) return -1;
  p = strchr (type, ';');
  while (p && *p) {
    const char *key, *end, *value;
    size_t key_length, n;
    ++p; while (*p == ' ' || *p == '\t') ++p;
    key = p; while (*p && *p != '=' && *p != ';' && *p != ' ' && *p != '\t') ++p;
    key_length = (size_t)(p - key);
    while (*p == ' ' || *p == '\t') ++p;
    if (*p++ != '=') return -1;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == '"') { value = ++p; while (*p && *p != '"') ++p; if (!*p) return -1; end = p++; }
    else { value = p; while (*p && *p != ';' && *p != ' ' && *p != '\t') ++p; end = p; }
    while (*p == ' ' || *p == '\t') ++p;
    if (*p && *p != ';') return -1;
    if (key_length == 8) {
      char key_text[9]; memcpy (key_text, key, 8); key_text[8] = 0;
      if (mulib_openai_server_equal_ (key_text, "boundary")) {
        n = (size_t)(end - value);
        if (found++ || !n || n > 70) return -1;
        memcpy (boundary, value, n); boundary[n] = 0;
      }
    }
  }
  if (!found) return -1;
  return mulib_multipart_next (boundary, q->http->body, q->http->body_length, 32768, cursor, part);
}
static int mulib_openai_server_credential_ (const mulib_http_message_t *m, const char *key)
{
  const char *value = mulib_http_header (m, "Authorization");
  size_t n = strlen (key);
  unsigned difference = 0;
  if (!value || strncmp (value, "Bearer ", 7) || strlen (value + 7) != n || !n) return 0;
  for (size_t i = 0; i < n; ++i) difference |= (unsigned char)value[i + 7] ^ (unsigned char)key[i];
  return difference == 0;
}
int mulib_openai_server_dispatch (const mulib_openai_server_t *server, const mulib_http_message_t *m,
    mulib_openai_write_fn write, void *user)
{
  mulib_openai_server_t defaults = mulib_openai_server_default (NULL, NULL);
  const mulib_openai_server_t *s = server ? server : &defaults;
  mulib_openai_writer_t w;
  mulib_openai_server_request_t q;
  mulib_json_t *json = NULL;
  const char *base = s->base_path ? s->base_path : "/v1", *type, *encoding;
  size_t n = strlen (base), cursor = 0;
  int rc;
  if (!m || !write || m->is_response || !m->method || !m->target
      || (!m->headers && m->header_count) || (!m->body && m->body_length)
      || (n && (base[0] != '/' || base[n - 1] == '/' || strpbrk (base, "?#")))) return -1;
  memset (&w, 0, sizeof w); w.write = write; w.user = user; w.limits = s->limits; w.max_event_bytes = s->max_event_bytes;
  memset (&q, 0, sizeof q); q.http = m;
  if (m->body_length > (s->limits.max_body_bytes ? s->limits.max_body_bytes : 16u * 1024u * 1024u))
    return mulib_openai_server_error (&w, 413, "Request body too large", "invalid_request_error", NULL, "body_too_large");
  if (strncmp (m->target, base, n) || m->target[n] != '/')
    return mulib_openai_server_error (&w, 404, "Unknown endpoint", "invalid_request_error", NULL, "not_found");
  q.target = m->target + n; q.operation = mulib_openai_match (m->method, q.target);
  if (!q.operation) {
    unsigned status = mulib_openai_match (NULL, q.target) ? 405 : 404;
    if (status == 405) {
      static const char *methods[] = {"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"};
      mulib_strbuf_t allow = {0};
      mulib_http_header_t header;
      mulib_json_t *error;
      char *text;
      for (size_t i = 0; i < sizeof methods / sizeof methods[0]; ++i) {
        if (mulib_openai_match (methods[i], q.target)) {
          if ((allow.length && !mulib_strbuf_append (&allow, ", ")) || !mulib_strbuf_append (&allow, methods[i])) {
            mulib_strbuf_free (&allow); return -3;
          }
        }
      }
      error = mulib_openai_error ("Method not allowed", "invalid_request_error", NULL, "method_not_allowed");
      text = error ? mulib_json_stringify (error, 0) : NULL;
      header.name = (char *)"Allow"; header.value = allow.data;
      rc = text ? mulib_openai_server_reply (&w, 405, "application/json", &header, 1, text, strlen (text)) : -3;
      free (text); mulib_json_destroy (error); mulib_strbuf_free (&allow); return rc;
    }
    return mulib_openai_server_error (&w, status, status == 405 ? "Method not allowed" : "Unknown endpoint",
        "invalid_request_error", NULL, status == 405 ? "method_not_allowed" : "not_found");
  }
  (void)mulib_http_header_next (m, "Authorization", &cursor);
  if (mulib_http_header_next (m, "Authorization", &cursor))
    return mulib_openai_server_error (&w, 400, "Duplicate Authorization", "invalid_request_error", NULL, NULL);
  if (s->api_key && !mulib_openai_server_credential_ (m, s->api_key))
    return mulib_openai_server_error (&w, 401, "Invalid API key", "authentication_error", NULL, "invalid_api_key");
  if (s->authorize && s->authorize (s->user, &q) != 1)
    return mulib_openai_server_error (&w, 403, "Access denied", "permission_error", NULL, "permission_denied");
  cursor = 0; type = mulib_http_header_next (m, "Content-Type", &cursor);
  if (mulib_http_header_next (m, "Content-Type", &cursor))
    return mulib_openai_server_error (&w, 400, "Duplicate Content-Type", "invalid_request_error", NULL, NULL);
  encoding = mulib_http_header (m, "Content-Encoding");
  if ((encoding && !mulib_openai_server_equal_ (encoding, "identity"))
      || (m->body_length && !mulib_openai_accepts (q.operation, type)))
    return mulib_openai_server_error (&w, 415, "Unsupported content type or encoding", "invalid_request_error", NULL, NULL);
  if (!m->body_length && q.operation->body_required)
    return mulib_openai_server_error (&w, 400, "Request body required", "invalid_request_error", NULL, NULL);
  if (m->body_length && mulib_openai_media_type (type, "application/json")) {
    json = mulib_json_parse ((const char *)m->body, m->body_length, NULL);
    if (!json || mulib_json_typeof (json) != MULIB_JSON_OBJECT) {
      mulib_json_destroy (json);
      return mulib_openai_server_error (&w, 400, "Expected a JSON object", "invalid_request_error", NULL, "invalid_json");
    }
  }
  if (m->body_length && mulib_openai_media_type (type, "multipart/form-data")) {
    mulib_multipart_part_t part = {0};
    cursor = 0;
    while ((rc = mulib_openai_server_part (&q, &cursor, &part)) == 1) mulib_multipart_part_free (&part);
    if (rc != 0) return mulib_openai_server_error (&w, 400, "Invalid multipart body", "invalid_request_error", NULL, NULL);
  }
  q.json = json;
  rc = s->handler ? s->handler (s->user, &q, &w) : 0;
  mulib_json_destroy (json);
  if (w.error) return w.error;
  if (!w.state) return mulib_openai_server_error (&w, rc < 0 ? 500 : 501,
      rc < 0 ? "Handler failed" : "Operation not implemented", "server_error", NULL, NULL);
  if (rc != 1) return rc < 0 ? rc : -1;
  return w.state == 2 ? 1 : mulib_openai_server_done (&w, 0);
}
int mulib_openai_server_process (const mulib_openai_server_t *s, const void *bytes, size_t n,
    size_t *consumed, mulib_openai_write_fn write, void *user)
{
  mulib_http_message_t m = {0};
  mulib_http_options_t limits = {0};
  size_t used = 0;
  int rc;
  if (consumed) *consumed = 0;
  if (!write) return -1;
  if (s) limits = s->limits;
  rc = mulib_http_parse (bytes, n, 0, &limits, &m, &used);
  if (rc != 1) return rc;
  if (consumed) *consumed = used;
  rc = mulib_openai_server_dispatch (s, &m, write, user);
  mulib_http_message_free (&m); return rc;
}
#endif
