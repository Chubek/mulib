/** @file mulib-openai-client.h
 * @brief OpenAI REST client over MuW3: raw/catalog calls, live SSE, multipart,
 * pagination and bounded Responses tool workflows. Define MULIB_IMPL_OPENAI_CLIENT.
 * Dependencies: MUW3, OPENAI, SSE, MULTIPART and their implementation closures. */
#ifndef MULIB_OPENAI_CLIENT_H
#define MULIB_OPENAI_CLIENT_H
#include "mulib-muw3.h"
#include "mulib-openai.h"
#include "mulib-sse.h"
#include "mulib-multipart.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_openai_client {
  mulib_muw3_client_t transport;
  const char *base_url, *api_key, *organization, *project, *beta;
  size_t max_event_bytes;
} mulib_openai_client_t;
typedef struct mulib_openai_event {
  mulib_sse_event_t event;
  const mulib_json_t *json; /* NULL for [DONE]; borrowed during callback */
  int done;
} mulib_openai_event_t;
typedef int (*mulib_openai_event_fn) (void *, const mulib_openai_event_t *);
typedef struct mulib_openai_request {
  const char *operation_id; /* catalog lookup, or NULL for raw method/target */
  const char *method, *target; /* raw target starts '/', relative to base_url */
  const mulib_http_header_t *path, *query, *headers;
  size_t path_count, query_count, header_count;
  const mulib_json_t *json; /* mutually exclusive with body */
  const void *body;
  size_t body_length;
  const char *content_type;
  mulib_openai_event_fn event; /* live SSE; NULL for buffered/binary */
  void *event_user;
} mulib_openai_request_t;
typedef struct mulib_openai_response {
  mulib_muw3_response_t http;
  mulib_json_t *json;
  int stream_done; /* [DONE] or terminal Responses event observed */
} mulib_openai_response_t;
/** Borrowed configuration, default https://api.openai.com/v1; no redirects or
 * automatic retries. API key may be NULL for local servers. */
mulib_openai_client_t mulib_openai_client_default (const char *api_key);
/** OK and API_ERROR transfer owned response (including HTTP status/body/headers).
 * Other errors leave out unchanged. An existing output must be freed first.
 * Events are delivered before final HTTP validation; callback 0 cancels. */
int mulib_openai_client_request (const mulib_openai_client_t *,
    const mulib_openai_request_t *, mulib_openai_response_t *);
void mulib_openai_response_free (mulib_openai_response_t *);
/** Simple named JSON call; use request for path/query parameters/streaming. */
int mulib_openai_client_json (const mulib_openai_client_t *, const char *operation_id,
    const mulib_json_t *, mulib_openai_response_t *);
/** Encode binary multipart and perform a catalog request. request's body/json/
 * content_type must be empty; other fields (including path/query) are preserved. */
int mulib_openai_client_multipart (const mulib_openai_client_t *,
    const mulib_openai_request_t *, const char *boundary,
    const mulib_multipart_part_t *, size_t count, mulib_openai_response_t *);
/** Owned request builders. Add arbitrary protocol fields with mulib_json_set. */
mulib_json_t *mulib_openai_responses_request (const char *model, const char *input);
mulib_json_t *mulib_openai_chat_request (const char *model, const char *user_text);
mulib_json_t *mulib_openai_embeddings_request (const char *model, const char *input);
typedef int (*mulib_openai_item_fn) (void *, const mulib_json_t *item);
/** Cursor pagination for data[]/has_more/last_id lists using after. max_pages
 * must be nonzero. Callback 0 cancels. final owns the last HTTP response on
 * OK/API_ERROR/LIMIT/CANCELLED/PROTOCOL after any successful transaction.
 * Does not mutate the request. Unknown pagination envelopes return PROTOCOL. */
int mulib_openai_client_pages (const mulib_openai_client_t *, const mulib_openai_request_t *,
    size_t max_pages, mulib_openai_item_fn, void *, mulib_openai_response_t *final);
/** Tool handler receives a Responses function_call item and parsed arguments.
 * Return 1 with an owned JSON STRING output, 0 to cancel, or a negative result.
 * Handler owns side effects; calls are never retried. */
typedef int (*mulib_openai_tool_fn) (void *, const mulib_json_t *call,
                                    const mulib_json_t *arguments, mulib_json_t **output);
/** Bounded sequential tool loop using previous_response_id. initial must be a
 * nonstreaming Responses request (store must not be false). Preserves initial
 * model/tools/instructions/options. max_rounds counts HTTP requests. final owns
 * the latest response whenever one was received. */
int mulib_openai_client_run (const mulib_openai_client_t *, const mulib_json_t *initial,
    size_t max_rounds, mulib_openai_tool_fn, void *, mulib_openai_response_t *final);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OPENAI_CLIENT) && !defined(MULIB_OPENAI_CLIENT_IMPLEMENTED)
#define MULIB_OPENAI_CLIENT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
mulib_openai_client_t mulib_openai_client_default (const char *key)
{
  mulib_openai_client_t c;
  memset (&c, 0, sizeof c); c.transport = mulib_muw3_client_default ();
  c.transport.options.max_redirects = 0;
  c.base_url = "https://api.openai.com/v1"; c.api_key = key; c.max_event_bytes = 1048576;
  return c;
}
void mulib_openai_response_free (mulib_openai_response_t *r)
{ if (r) { mulib_muw3_response_free (&r->http); mulib_json_destroy (r->json); memset (r, 0, sizeof *r); } }
static int mulib_openai_client_equal_ (const char *a, const char *b)
{
  if (!a || !b) return 0;
  while (*a && *b) {
    unsigned char c = (unsigned char)*a++, d = (unsigned char)*b++;
    if (c >= 'A' && c <= 'Z') c += 32;
    if (d >= 'A' && d <= 'Z') d += 32;
    if (c != d) return 0;
  }
  return *a == *b;
}
typedef struct mulib_openai_client_stream_ {
  mulib_sse_parser_t *parser;
  mulib_openai_event_fn callback;
  void *user;
  mulib_buffer_t error_body;
  int error, enabled, done;
} mulib_openai_client_stream_;
static int mulib_openai_client_event_ (void *user, const mulib_sse_event_t *event)
{
  mulib_openai_client_stream_ *s = (mulib_openai_client_stream_ *)user;
  mulib_openai_event_t e;
  mulib_json_t *json = NULL;
  const char *type;
  int ok;
  memset (&e, 0, sizeof e); e.event = *event;
  e.done = event->data_length == 6 && !memcmp (event->data, "[DONE]", 6);
  if (s->done) { s->error = -9; return 0; }
  if (!e.done) {
    json = mulib_json_parse (event->data, event->data_length, NULL);
    if (!json) { s->error = -9; return 0; }
    type = mulib_json_text (mulib_json_get (json, "type"), NULL);
    if (type && (!strcmp (type, "response.completed") || !strcmp (type, "response.failed")
        || !strcmp (type, "response.incomplete"))) e.done = 1;
  }
  e.json = json; s->done = e.done;
  ok = s->callback (s->user, &e); mulib_json_destroy (json);
  if (ok != 1) s->error = -7;
  return ok == 1;
}
static int mulib_openai_client_body_ (void *user, const mulib_http_message_t *m,
    const void *bytes, size_t n)
{
  mulib_openai_client_stream_ *s = (mulib_openai_client_stream_ *)user;
  if (!bytes) {
    s->enabled = m->status >= 200 && m->status < 300;
    if (s->enabled && !mulib_openai_media_type (mulib_http_header (m, "Content-Type"), "text/event-stream")) {
      s->error = -9; return 0;
    }
    if (mulib_http_header (m, "Content-Encoding")
        && !mulib_openai_client_equal_ (mulib_http_header (m, "Content-Encoding"), "identity")) {
      s->error = -9; return 0;
    }
    return 1;
  }
  if (s->enabled) {
    int rc = mulib_sse_feed (s->parser, bytes, n);
    if (rc != 1 && !s->error) s->error = rc;
  } else if (!mulib_buffer_append (&s->error_body, bytes, n)) s->error = -3;
  return !s->error;
}
int mulib_openai_client_request (const mulib_openai_client_t *client,
    const mulib_openai_request_t *q, mulib_openai_response_t *out)
{
  mulib_openai_client_t defaults = mulib_openai_client_default (NULL);
  const mulib_openai_client_t *c = client ? client : &defaults;
  const mulib_openai_operation_t *op = NULL;
  mulib_openai_response_t r = {0};
  mulib_openai_client_stream_ stream = {0};
  mulib_muw3_request_t req = {0};
  mulib_muw3_client_t transport = c->transport;
  mulib_uri_t uri;
  mulib_strbuf_t url = {0}, auth = {0};
  mulib_http_header_t *headers = NULL;
  char *target = NULL, *body = NULL;
  const char *base = c->base_url ? c->base_url : defaults.base_url;
  const char *type;
  size_t base_length, count = 0, max_url = transport.options.max_url_bytes ? transport.options.max_url_bytes : 8192;
  int rc = -1;
  if (!q || !out || (q->json && (q->body || q->body_length)) || (!q->body && q->body_length)
      || (!q->headers && q->header_count) || (!q->query && q->query_count)
      || (!q->path && q->path_count)) return -1;
  if (q->header_count > 1000 || max_url > 1048576) return -2;
  if (q->operation_id) {
    op = mulib_openai_operation (q->operation_id);
    if (!op) return -10;
    if (q->method || q->target) return -1;
    rc = mulib_openai_target (op, q->path, q->path_count, q->query, q->query_count, max_url, &target);
    if (rc != 1) return rc;
    req.method = op->method;
  } else {
    mulib_openai_operation_t raw;
    if (!q->target || !q->method || q->path_count || strchr (q->target, '{') || strchr (q->target, '}')) return -1;
    memset (&raw, 0, sizeof raw); raw.path = q->target;
    rc = mulib_openai_target (&raw, NULL, 0, q->query, q->query_count, max_url, &target);
    if (rc != 1) return rc;
    req.method = q->method;
  }
  rc = -1;
  if (!mulib_uri_parse (target, strlen (target), &uri) || uri.has_authority || uri.has_fragment
      || uri.scheme.length || !uri.path.length || uri.path.data[0] != '/') goto done;
  base_length = strlen (base);
  if (base_length > max_url || strlen (target) > max_url - base_length) { rc = -2; goto done; }
  if (!mulib_uri_parse (base, base_length, &uri) || !uri.has_authority || uri.has_userinfo
      || uri.has_fragment || uri.has_query) goto done;
  while (base_length && base[base_length - 1] == '/') --base_length;
  rc = -3;
  if (!mulib_strbuf_append_n (&url, base, base_length) || !mulib_strbuf_append (&url, target)) goto done;
  req.url = url.data;
  type = q->json ? "application/json" : q->content_type;
  if (q->json) {
    if (q->content_type && !mulib_openai_media_type (q->content_type, "application/json")) { rc = -1; goto done; }
    body = mulib_json_stringify (q->json, 0); if (!body) goto done;
    req.body = body; req.body_length = strlen (body);
  } else { req.body = q->body; req.body_length = q->body_length; }
  if (op && ((op->body_required && !req.body_length)
      || (req.body_length && !mulib_openai_accepts (op, type)))) { rc = -1; goto done; }
  headers = (mulib_http_header_t *)calloc (q->header_count + 7, sizeof *headers);
  if (!headers) goto done;
  for (size_t i = 0; i < q->header_count; ++i) {
    const char *name = q->headers[i].name;
    if (!name || !q->headers[i].value || mulib_openai_client_equal_ (name, "Authorization")
        || mulib_openai_client_equal_ (name, "Content-Type") || mulib_openai_client_equal_ (name, "Content-Length")
        || mulib_openai_client_equal_ (name, "Transfer-Encoding") || mulib_openai_client_equal_ (name, "OpenAI-Organization")
        || mulib_openai_client_equal_ (name, "OpenAI-Project") || mulib_openai_client_equal_ (name, "OpenAI-Beta")
        || mulib_openai_client_equal_ (name, "Accept") || mulib_openai_client_equal_ (name, "Accept-Encoding")) { rc = -1; goto done; }
    headers[count++] = q->headers[i];
  }
#define MULIB_OPENAI_HEADER_(n,v) do { headers[count].name = (char *)(n); headers[count++].value = (char *)(v); } while (0)
  if (c->api_key) {
    if (!*c->api_key) { rc = -1; goto done; }
    if (!mulib_strbuf_append (&auth, "Bearer ") || !mulib_strbuf_append (&auth, c->api_key)) goto done;
    MULIB_OPENAI_HEADER_("Authorization",auth.data);
  }
  if (c->organization) { MULIB_OPENAI_HEADER_("OpenAI-Organization",c->organization); }
  if (c->project) { MULIB_OPENAI_HEADER_("OpenAI-Project",c->project); }
  if (c->beta) { MULIB_OPENAI_HEADER_("OpenAI-Beta",c->beta); }
  if (type) { MULIB_OPENAI_HEADER_("Content-Type",type); }
  MULIB_OPENAI_HEADER_("Accept",q->event ? "text/event-stream" : "*/*");
  MULIB_OPENAI_HEADER_("Accept-Encoding","identity");
#undef MULIB_OPENAI_HEADER_
  req.headers = headers; req.header_count = count;
  /* Preserve authentication scope and POST semantics even if caller supplied
   * a MuW3 configuration with redirects enabled. */
  transport.options.max_redirects = 0;
  if (q->event) {
    stream.callback = q->event; stream.user = q->event_user;
    stream.parser = mulib_sse_create (c->max_event_bytes, mulib_openai_client_event_, &stream);
    if (!stream.parser) { rc = -3; goto done; }
    rc = mulib_muw3_stream (&transport, &req, mulib_openai_client_body_, &stream, &r.http);
    if (stream.error) rc = stream.error;
    if (rc != 1) goto done;
    rc = mulib_sse_finish (stream.parser); if (rc != 1) goto done;
    r.stream_done = stream.done;
    r.http.message.body = stream.error_body.data; r.http.message.body_length = stream.error_body.length;
    memset (&stream.error_body, 0, sizeof stream.error_body);
  } else {
    rc = mulib_muw3_perform (&transport, &req, &r.http);
    if (rc != 1) goto done;
  }
  rc = r.http.message.status >= 200 && r.http.message.status < 300 ? 1 : -8;
  if (r.http.message.body_length && mulib_openai_media_type (mulib_http_header (&r.http.message, "Content-Type"), "application/json")) {
    r.json = mulib_json_parse ((const char *)r.http.message.body, r.http.message.body_length, NULL);
    if (!r.json && rc == 1) { rc = -9; goto done; }
  }
  *out = r; memset (&r, 0, sizeof r);
done:
  mulib_sse_destroy (stream.parser); mulib_buffer_free (&stream.error_body);
  mulib_openai_response_free (&r); free (target); free (body); free (headers);
  mulib_strbuf_free (&url); mulib_strbuf_free (&auth); return rc;
}
int mulib_openai_client_json (const mulib_openai_client_t *c, const char *id,
    const mulib_json_t *json, mulib_openai_response_t *out)
{
  mulib_openai_request_t q = {0}; q.operation_id = id; q.json = json;
  return mulib_openai_client_request (c, &q, out);
}
int mulib_openai_client_multipart (const mulib_openai_client_t *c, const mulib_openai_request_t *request,
    const char *boundary, const mulib_multipart_part_t *parts, size_t count, mulib_openai_response_t *out)
{
  mulib_openai_request_t q;
  unsigned char *body = NULL;
  size_t n, limit = c ? c->transport.options.web.http.max_body_bytes : 0;
  char type[110];
  int rc;
  if (!request || request->body || request->body_length || request->json || request->content_type) return -1;
  rc = mulib_multipart_encode (boundary, parts, count, limit, &body, &n);
  if (rc != 1) return rc;
  memcpy (type, "multipart/form-data; boundary=", 30);
  memcpy (type + 30, boundary, strlen (boundary) + 1);
  q = *request; q.body = body; q.body_length = n; q.content_type = type;
  rc = mulib_openai_client_request (c, &q, out); free (body); return rc;
}
static int mulib_openai_client_set_ (mulib_json_t *o, const char *key, mulib_json_t *v)
{
  if (v && mulib_json_set (o, key, v)) return 1;
  mulib_json_destroy (v); return 0;
}
mulib_json_t *mulib_openai_responses_request (const char *model, const char *input)
{
  mulib_json_t *o;
  if (!model || !*model || !input) return NULL;
  o = mulib_json_new (MULIB_JSON_OBJECT);
  if (o && mulib_openai_client_set_ (o, "model", mulib_json_string (model))
      && mulib_openai_client_set_ (o, "input", mulib_json_string (input))) return o;
  mulib_json_destroy (o); return NULL;
}
mulib_json_t *mulib_openai_embeddings_request (const char *model, const char *input)
{ return mulib_openai_responses_request (model, input); }
mulib_json_t *mulib_openai_chat_request (const char *model, const char *text)
{
  mulib_json_t *o = NULL, *a = NULL, *m = NULL;
  if (!model || !*model || !text) return NULL;
  o = mulib_json_new (MULIB_JSON_OBJECT); a = mulib_json_new (MULIB_JSON_ARRAY); m = mulib_json_new (MULIB_JSON_OBJECT);
  if (!o || !a || !m || !mulib_openai_client_set_ (o, "model", mulib_json_string (model))
      || !mulib_openai_client_set_ (m, "role", mulib_json_string ("user"))
      || !mulib_openai_client_set_ (m, "content", mulib_json_string (text)) || !mulib_json_append (a, m)) goto fail;
  m = NULL;
  if (!mulib_json_set (o, "messages", a)) goto fail;
  return o;
fail:
  mulib_json_destroy (m); mulib_json_destroy (a); mulib_json_destroy (o); return NULL;
}
int mulib_openai_client_pages (const mulib_openai_client_t *c, const mulib_openai_request_t *request,
    size_t max_pages, mulib_openai_item_fn callback, void *user, mulib_openai_response_t *final)
{
  mulib_openai_request_t q;
  mulib_openai_response_t r = {0};
  mulib_http_header_t *query;
  char *cursor = NULL;
  size_t slot, count;
  int rc = -1, have = 0;
  if (!request || !callback || !final || !max_pages || request->event
      || (!request->query && request->query_count) || request->query_count > 1000) return -1;
  q = *request; count = q.query_count; slot = count;
  query = (mulib_http_header_t *)calloc (count + 1, sizeof *query);
  if (!query) return -3;
  for (size_t i = 0; i < count; ++i) {
    if (!q.query[i].name || !q.query[i].value) goto done;
    query[i] = q.query[i];
    if (!strcmp (query[i].name, "after")) { if (slot != count) goto done; slot = i; }
  }
  q.query = query;
  for (size_t page = 0; page < max_pages; ++page) {
    mulib_openai_response_t next = {0};
    mulib_json_t *items, *more;
    const char *id;
    size_t id_length;
    rc = mulib_openai_client_request (c, &q, &next);
    if (rc != 1 && rc != -8) break;
    mulib_openai_response_free (&r); r = next; have = 1;
    if (rc != 1) break;
    items = mulib_json_get (r.json, "data"); more = mulib_json_get (r.json, "has_more");
    if (mulib_json_typeof (items) != MULIB_JSON_ARRAY || mulib_json_typeof (more) != MULIB_JSON_BOOL) { rc = -9; break; }
    for (size_t i = 0; i < mulib_json_size (items); ++i)
      if (callback (user, mulib_json_at (items, i)) != 1) { rc = -7; goto done; }
    if (!mulib_json_boolean (more)) break;
    id = mulib_json_text (mulib_json_get (r.json, "last_id"), &id_length);
    if (!id) id = mulib_json_text (mulib_json_get (mulib_json_at (items, mulib_json_size (items) - 1), "id"), &id_length);
    if (!id || !id_length || strlen (id) != id_length
        || (query[slot].value && !strcmp (query[slot].value, id))) { rc = -9; break; }
    if (page + 1 == max_pages) { rc = -2; break; }
    free (cursor); cursor = (char *)malloc (id_length + 1);
    if (!cursor) { rc = -3; break; }
    memcpy (cursor, id, id_length + 1);
    query[slot].name = (char *)"after"; query[slot].value = cursor;
    q.query_count = count + (slot == count);
  }
done:
  if (have) *final = r;
  free (cursor); free (query); return rc;
}
int mulib_openai_client_run (const mulib_openai_client_t *c, const mulib_json_t *initial,
    size_t max_rounds, mulib_openai_tool_fn tool, void *user, mulib_openai_response_t *final)
{
  mulib_json_t *request = NULL;
  mulib_openai_response_t r = {0};
  int rc = -1, have = 0;
  if (!final || !max_rounds || !tool || mulib_json_typeof (initial) != MULIB_JSON_OBJECT
      || mulib_json_boolean (mulib_json_get (initial, "stream"))
      || (mulib_json_get (initial, "store") && !mulib_json_boolean (mulib_json_get (initial, "store")))) return -1;
  request = mulib_openai_clone (initial); if (!request) return -3;
  for (size_t round = 0; round < max_rounds; ++round) {
    mulib_openai_response_t next = {0};
    mulib_json_t *items, *outputs;
    const char *id;
    size_t calls = 0, id_length;
    rc = mulib_openai_client_json (c, "createResponse", request, &next);
    if (rc != 1 && rc != -8) break;
    mulib_openai_response_free (&r); r = next; have = 1;
    if (rc != 1) break;
    items = mulib_json_get (r.json, "output");
    if (mulib_json_typeof (items) != MULIB_JSON_ARRAY) { rc = -9; break; }
    for (size_t i = 0; i < mulib_json_size (items); ++i) {
      const char *type = mulib_json_text (mulib_json_get (mulib_json_at (items, i), "type"), NULL);
      if (type && !strcmp (type, "function_call")) ++calls;
    }
    if (!calls) break;
    if (round + 1 == max_rounds) { rc = -2; break; }
    id = mulib_json_text (mulib_json_get (r.json, "id"), &id_length);
    if (!id || !id_length || strlen (id) != id_length) { rc = -9; break; }
    outputs = mulib_json_new (MULIB_JSON_ARRAY);
    if (!outputs) { rc = -3; break; }
    for (size_t i = 0; i < mulib_json_size (items); ++i) {
      mulib_json_t *call = mulib_json_at (items, i), *args, *value = NULL, *entry;
      const char *type = mulib_json_text (mulib_json_get (call, "type"), NULL), *arguments;
      size_t length;
      if (!type || strcmp (type, "function_call")) continue;
      arguments = mulib_json_text (mulib_json_get (call, "arguments"), &length);
      args = arguments ? mulib_json_parse (arguments, length, NULL) : NULL;
      if (!args || mulib_json_typeof (mulib_json_get (call, "call_id")) != MULIB_JSON_STRING) {
        mulib_json_destroy (args); rc = -9; goto outputs_done;
      }
      rc = tool (user, call, args, &value); mulib_json_destroy (args);
      if (rc != 1 || mulib_json_typeof (value) != MULIB_JSON_STRING) {
        mulib_json_destroy (value); rc = rc == 0 ? -7 : (rc == 1 ? -1 : rc); goto outputs_done;
      }
      entry = mulib_json_new (MULIB_JSON_OBJECT);
      if (!entry || !mulib_openai_client_set_ (entry, "type", mulib_json_string ("function_call_output"))
          || !mulib_openai_client_set_ (entry, "call_id", mulib_openai_clone (mulib_json_get (call, "call_id")))) {
        mulib_json_destroy (entry); mulib_json_destroy (value); rc = -3; goto outputs_done;
      }
      if (!mulib_openai_client_set_ (entry, "output", value)) { mulib_json_destroy (entry); rc = -3; goto outputs_done; }
      if (!mulib_json_append (outputs, entry)) { mulib_json_destroy (entry); rc = -3; goto outputs_done; }
    }
    if (!mulib_openai_client_set_ (request, "previous_response_id", mulib_json_string (id))) { rc = -3; goto outputs_done; }
    if (!mulib_json_set (request, "input", outputs)) { rc = -3; goto outputs_done; }
    continue;
outputs_done:
    mulib_json_destroy (outputs); break;
  }
  mulib_json_destroy (request); if (have) *final = r; return rc;
}
#endif
