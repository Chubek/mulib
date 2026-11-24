/** @file mune-opencode-client.h
 * @brief OpenCode V2 client implementation; also a standalone include entry. */
#ifndef MULIB_MUNE_OPENCODE_CLIENT_H
#include "mulib-opencode-client.h"
#endif

#if defined(MULIB_IMPL_CDD_MUNE_OPENCODE_CLIENT) && !defined(MULIB_MUNE_OPENCODE_CLIENT_IMPLEMENTED)
#define MULIB_MUNE_OPENCODE_CLIENT_IMPLEMENTED
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

enum { moc_url_cap_ = 8192, moc_fields_ = 64, moc_event_cap_ = 1048576,
       moc_body_cap_ = 8388608, moc_wire_cap_ = moc_body_cap_ + 262144 };
static const mulib_cdd_opencode_operation_t moc_operations_[] = {
#include "detail/mulib-opencode-operations.inc"
};

size_t mulib_cdd_opencode_operation_count(void)
{ return sizeof moc_operations_ / sizeof *moc_operations_; }
const mulib_cdd_opencode_operation_t *mulib_cdd_opencode_operation_at(size_t i)
{ return i < mulib_cdd_opencode_operation_count() ? &moc_operations_[i] : NULL; }
const mulib_cdd_opencode_operation_t *mulib_cdd_opencode_operation(const char *id)
{
  size_t lo = 0, hi = mulib_cdd_opencode_operation_count();
  if (!id) return NULL;
  while (lo < hi) {
    size_t i = lo + (hi - lo) / 2;
    int cmp = strcmp(id, moc_operations_[i].id);
    if (!cmp) return &moc_operations_[i];
    if (cmp < 0) hi = i; else lo = i + 1;
  }
  return NULL;
}
mulib_cdd_opencode_client_t mulib_cdd_opencode_client(const char *origin)
{
  mulib_cdd_opencode_client_t c;
  memset(&c, 0, sizeof c);
  c.transport = mulib_muw3_client_default();
  c.transport.options.max_redirects = 0;
  c.origin = origin;
  return c;
}
void mulib_cdd_opencode_response_free(mulib_cdd_opencode_response_t *r)
{
  if (!r) return;
  mulib_muw3_response_free(&r->http);
  mulib_json_destroy(r->json);
  r->json = NULL;
}

static size_t moc_len_(const char *s, size_t limit)
{
  size_t n = 0;
  if (!s) return SIZE_MAX;
  while (n <= limit && s[n]) ++n;
  return n <= limit ? n : SIZE_MAX;
}
static int moc_eq_(const char *s, size_t n, const char *literal)
{ return n == strlen(literal) && !memcmp(s, literal, n); }
static int moc_ci_(const char *s, size_t n, const char *lower)
{
  if (n != strlen(lower)) return 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + ('a' - 'A'));
    if (c != (unsigned char)lower[i]) return 0;
  }
  return 1;
}
static int moc_media_(const char *s, const char *lower)
{
  size_t n;
  if (!s) return 0;
  n = strcspn(s, "; \t");
  if (!moc_ci_(s, n, lower)) return 0;
  while (s[n] == ' ' || s[n] == '\t') ++n;
  return s[n] == 0 || s[n] == ';';
}
static int moc_origin_(const mulib_cdd_opencode_client_t *c, size_t *length)
{
  size_t n = moc_len_(c->origin, 1024);
  mulib_uri_t uri;
  if (n == SIZE_MAX || !n || strpbrk(c->origin, "\\% \r\n\t") ||
      !mulib_uri_parse(c->origin, n, &uri) || !uri.has_authority ||
      !uri.host.length || uri.has_userinfo || uri.has_query || uri.has_fragment ||
      (uri.path.length && !moc_eq_(uri.path.data, uri.path.length, "/"))) return 0;
  if (!moc_ci_(uri.scheme.data, uri.scheme.length, "https") &&
      (!c->allow_http || !moc_ci_(uri.scheme.data, uri.scheme.length, "http"))) return 0;
  if (uri.has_port) {
    unsigned port = 0;
    if (!uri.port.length) return 0;
    for (size_t i = 0; i < uri.port.length; i++) {
      unsigned d = (unsigned char)uri.port.data[i] - (unsigned)'0';
      if (d > 9 || port > (65535u - d) / 10u) return 0;
      port = port * 10u + d;
    }
    if (!port) return 0;
  }
  *length = n - (c->origin[n-1] == '/');
  return 1;
}
static int moc_append_(char *out, size_t *at, const char *p, size_t n)
{
  if (n >= moc_url_cap_ - *at) return 0;
  memcpy(out + *at, p, n);
  *at += n;
  out[*at] = 0;
  return 1;
}
static int moc_encode_(char *out, size_t *at, const char *s, int path)
{
  size_t n = moc_len_(s, moc_url_cap_), written;
  if (n == SIZE_MAX) return s ? -2 : -1;
  if (path && (!n || !strcmp(s, ".") || !strcmp(s, "..") || strpbrk(s, "/\\%"))) return -1;
  if (!mulib_utf8_validate((const unsigned char *)s, n)) return -1;
  for (size_t i = 0; i < n; i++) if ((unsigned char)s[i] < 32 || s[i] == 127) return -1;
  written = mulib_url_encode((const unsigned char *)s, n, out + *at, moc_url_cap_ - *at);
  if (!written && n) return -2;
  *at += written;
  return 1;
}
static int moc_query_rule_(const char *list, const char *key)
{
  while (*list) {
    size_t n = strcspn(list, ","), m = n;
    int multi = list[n-1] == '*';
    if (multi || list[n-1] == '!') --m;
    if (moc_eq_(list, m, key)) return multi ? 2 : 1;
    list += n + (list[n] == ',');
  }
  return 0;
}
static int moc_target_(const mulib_cdd_opencode_client_t *c,
                       const mulib_cdd_opencode_operation_t *op,
                       const mulib_cdd_opencode_request_t *q, char out[moc_url_cap_])
{
  size_t at = 0, length, used = 0;
  const char *p = op->path;
  if (!moc_origin_(c, &length)) return -1;
  if (!moc_append_(out, &at, c->origin, length)) return -2;
  while (*p) {
    const char *end;
    size_t n = strcspn(p, "{"), i;
    int rc;
    if (!moc_append_(out, &at, p, n)) return -2;
    p += n;
    if (!*p) break;
    ++p;
    end = strchr(p, '}');
    if (!end) return -1;
    for (i = 0; i < q->path_count; i++)
      if (q->path[i].name && moc_eq_(p, (size_t)(end-p), q->path[i].name)) break;
    if (i == q->path_count) return -1;
    rc = moc_encode_(out, &at, q->path[i].value, 1);
    if (rc != 1) return rc;
    ++used;
    p = end + 1;
  }
  if (used != q->path_count) return -1; /* includes duplicate or extra path values */
  for (size_t i = 0; i < q->query_count; i++) {
    const char *key = q->query[i].name;
    int rule, rc;
    if (!key || !(rule = moc_query_rule_(op->query_names, key))) return -1;
    if (rule == 1) for (size_t j = 0; j < i; j++)
      if (!strcmp(q->query[j].name, key)) return -1;
    if (!moc_append_(out, &at, i ? "&" : "?", 1)) return -2;
    rc = moc_encode_(out, &at, key, 0);
    if (rc != 1) return rc;
    if (!moc_append_(out, &at, "=", 1)) return -2;
    rc = moc_encode_(out, &at, q->query[i].value, 0);
    if (rc != 1) return rc;
  }
  p = op->query_names;
  while (*p) {
    size_t n = strcspn(p, ",");
    if (p[n-1] == '!') {
      size_t i = 0;
      while (i < q->query_count && !moc_eq_(p, n-1, q->query[i].name)) ++i;
      if (i == q->query_count || !q->query[i].value[0]) return -1;
    }
    p += n + (p[n] == ',');
  }
  return 1;
}

static int moc_charge_(size_t *left, size_t n)
{
  if (n > *left) return 0;
  *left -= n;
  return 1;
}
/* Conservative JSON serialization cost, including escape expansion. */
static int moc_measure_(const mulib_json_t *v, size_t *left, unsigned depth)
{
  mulib_json_type kind = mulib_json_typeof(v);
  size_t n;
  if (depth > 64 || !moc_charge_(left, 6)) return 0;
  if (kind == MULIB_JSON_STRING) {
    (void)mulib_json_text(v, &n);
    return n <= *left / 6 && moc_charge_(left, n * 6);
  }
  if (kind == MULIB_JSON_NUMBER) return moc_charge_(left, 32);
  for (size_t i = 0; i < mulib_json_size(v); i++) {
    if (kind == MULIB_JSON_OBJECT) {
      (void)mulib_json_key(v, i, &n);
      if (!moc_charge_(left, 4) || n > *left / 6 || !moc_charge_(left, n * 6)) return 0;
    }
    if (!moc_measure_(mulib_json_at(v, i), left, depth + 1)) return 0;
  }
  return 1;
}
static int moc_body_(const mulib_cdd_opencode_operation_t *op, const mulib_json_t *v)
{
  const char *p = op->required_fields;
  if (!(op->flags & 1u)) return !v;
  if (mulib_json_typeof(v) != MULIB_JSON_OBJECT) return 0;
  while (*p) {
    size_t n = strcspn(p, ",");
    if (!mulib_json_get_n(v, p, n)) return 0;
    p += n + (p[n] == ',');
  }
  if (!strcmp(op->id, "session.prompt") &&
      mulib_json_typeof(mulib_json_get(v, "text")) != MULIB_JSON_STRING) return 0;
  return !mulib_json_get(v, "permissions") && !mulib_json_get(v, "parentID");
}
static int moc_encoding_(const mulib_http_message_t *m, const char **type)
{
  size_t cursor = 0;
  const char *value = mulib_http_header_next(m, "Content-Encoding", &cursor);
  if (value && (!moc_ci_(value, strlen(value), "identity") ||
                 mulib_http_header_next(m, "Content-Encoding", &cursor))) return 0;
  cursor = 0;
  *type = mulib_http_header_next(m, "Content-Type", &cursor);
  return !mulib_http_header_next(m, "Content-Type", &cursor);
}
typedef struct moc_stream_ {
  const mulib_cdd_opencode_request_t *request;
  mulib_sse_parser_t *parser;
  mulib_buffer_t error_body;
  size_t error_limit;
  unsigned success;
  int error, enabled;
} moc_stream_;
static int moc_event_(void *user, const mulib_sse_event_t *event)
{
  moc_stream_ *s = (moc_stream_ *)user;
  mulib_json_t *json;
  if (!strcmp(event->event, "effect/http-api/stream/failure")) {
    s->error = MULIB_CDD_OPENCODE_STREAM_ERROR;
    return 0;
  }
  json = mulib_json_parse(event->data, event->data_length, NULL);
  if (mulib_json_typeof(json) != MULIB_JSON_OBJECT) s->error = MULIB_CDD_OPENCODE_PROTOCOL;
  else if (s->request->event(s->request->event_user, event, json) != 1)
    s->error = MULIB_CDD_OPENCODE_CANCELLED;
  mulib_json_destroy(json);
  return !s->error;
}
static int moc_stream_feed_(void *user, const mulib_http_message_t *m, const void *data, size_t length)
{
  moc_stream_ *s = (moc_stream_ *)user;
  if (!data) {
    const char *type;
    s->enabled = m->status == s->success;
    if (!moc_encoding_(m, &type) || (s->enabled && !moc_media_(type, "text/event-stream")))
      s->error = MULIB_CDD_OPENCODE_PROTOCOL;
    return !s->error;
  }
  if (s->enabled) {
    int rc = mulib_sse_feed(s->parser, data, length);
    if (rc != 1 && !s->error) s->error = rc == -2 || rc == -3 ? rc : -9;
  } else if (length > s->error_limit - s->error_body.length) s->error = -2;
  else if (!mulib_buffer_append(&s->error_body, data, length)) s->error = -3;
  return !s->error;
}
static int moc_transport_(int rc)
{
  return rc == MULIB_MUW3_OK || rc == MULIB_MUW3_LIMIT || rc == MULIB_MUW3_NOMEM
    ? rc : MULIB_CDD_OPENCODE_TRANSPORT;
}
mulib_cdd_opencode_status_t mulib_cdd_opencode_request(
    const mulib_cdd_opencode_client_t *c, const mulib_cdd_opencode_request_t *q,
    mulib_cdd_opencode_response_t *out)
{
  const mulib_cdd_opencode_operation_t *op;
  mulib_cdd_opencode_response_t r;
  mulib_muw3_request_t request;
  mulib_muw3_client_t transport;
  mulib_http_header_t headers[3];
  char url[moc_url_cap_], *body = NULL;
  const char *type;
  size_t count = 1, bytes = moc_body_cap_, limit = moc_event_cap_;
  int rc = MULIB_CDD_OPENCODE_INVALID;
  if (!c || !q || !out || !q->operation_id || (!q->path && q->path_count) ||
      (!q->query && q->query_count)) return MULIB_CDD_OPENCODE_INVALID;
  if (q->path_count > moc_fields_ || q->query_count > moc_fields_)
    return MULIB_CDD_OPENCODE_LIMIT;
  op = mulib_cdd_opencode_operation(q->operation_id);
  if (!op || (strcmp(op->method, "GET") && !c->allow_session_writes))
    return MULIB_CDD_OPENCODE_UNSUPPORTED;
  if (!moc_body_(op, q->json) || (!!q->event != !!(op->flags & 2u)))
    return MULIB_CDD_OPENCODE_INVALID;
  rc = moc_target_(c, op, q, url);
  if (rc != 1) return (mulib_cdd_opencode_status_t)rc;
  headers[0].name = (char *)"Accept";
  headers[0].value = (char *)(op->flags & 2u ? "text/event-stream" : "application/json");
  if (c->authorization) {
    size_t n = moc_len_(c->authorization, 4096);
    if (n == SIZE_MAX || !n) return MULIB_CDD_OPENCODE_INVALID;
    for (size_t i = 0; i < n; i++)
      if ((unsigned char)c->authorization[i] < 32 || (unsigned char)c->authorization[i] >= 127)
        return MULIB_CDD_OPENCODE_INVALID;
    headers[count].name = (char *)"Authorization";
    headers[count++].value = (char *)c->authorization;
  }
  if (q->json) {
    if (!moc_measure_(q->json, &bytes, 0)) return MULIB_CDD_OPENCODE_LIMIT;
    body = mulib_json_stringify(q->json, 0);
    if (!body) return MULIB_CDD_OPENCODE_NOMEM;
    headers[count].name = (char *)"Content-Type";
    headers[count++].value = (char *)"application/json";
  }
  memset(&r, 0, sizeof r);
  memset(&request, 0, sizeof request);
  request.url = url; request.method = op->method;
  request.headers = headers; request.header_count = count;
  request.body = body; request.body_length = body ? strlen(body) : 0;
  transport = c->transport;
  transport.options.max_redirects = 0;
  if (!transport.options.max_url_bytes || transport.options.max_url_bytes > moc_url_cap_)
    transport.options.max_url_bytes = moc_url_cap_;
  if (!transport.options.web.http.max_body_bytes || transport.options.web.http.max_body_bytes > moc_body_cap_)
    transport.options.web.http.max_body_bytes = moc_body_cap_;
  if (!transport.options.web.max_response_bytes || transport.options.web.max_response_bytes > moc_wire_cap_)
    transport.options.web.max_response_bytes = moc_wire_cap_;
  if (strlen(url) > transport.options.max_url_bytes) { rc = -2; goto done; }
  if (op->flags & 2u) {
    moc_stream_ s;
    memset(&s, 0, sizeof s);
    s.request = q;
    s.success = op->success_status;
    s.error_limit = transport.options.web.http.max_body_bytes;
    mulib_buffer_init(&s.error_body);
    if (c->max_event_bytes && c->max_event_bytes < limit) limit = c->max_event_bytes;
    s.parser = mulib_sse_create(limit, moc_event_, &s);
    if (!s.parser) { rc = -3; goto done; }
    rc = moc_transport_(mulib_muw3_stream(&transport, &request, moc_stream_feed_, &s, &r.http));
    if (s.error) rc = s.error;
    else if (rc == 1 && s.enabled && mulib_sse_finish(s.parser) != 1) rc = -9;
    if (rc == 1 && !s.enabled) {
      r.http.message.body = s.error_body.data;
      r.http.message.body_length = s.error_body.length;
      s.error_body.data = NULL;
    }
    mulib_buffer_free(&s.error_body);
    mulib_sse_destroy(s.parser);
  } else rc = moc_transport_(mulib_muw3_perform(&transport, &request, &r.http));
  if (rc != 1) goto done;
  if (!moc_encoding_(&r.http.message, &type)) { rc = -9; goto done; }
  if (r.http.message.status != op->success_status) {
    if (r.http.message.status >= 200 && r.http.message.status < 300) { rc = -9; goto done; }
    if (r.http.message.body_length && moc_media_(type, "application/json"))
      r.json = mulib_json_parse((const char *)r.http.message.body, r.http.message.body_length, NULL);
    rc = MULIB_CDD_OPENCODE_HTTP_ERROR;
  } else if (op->success_status != 204 && !(op->flags & 2u)) {
    if (!moc_media_(type, "application/json") ||
        !(r.json = mulib_json_parse((const char *)r.http.message.body, r.http.message.body_length, NULL))) {
      rc = -9; goto done;
    }
  }
  *out = r;
  free(body);
  return (mulib_cdd_opencode_status_t)rc;
done:
  mulib_cdd_opencode_response_free(&r);
  free(body);
  return (mulib_cdd_opencode_status_t)rc;
}

static const char *moc_mune_string_(mulib_mune_value_t v)
{
  size_t n;
  const char *p = mulib_mune_value_string(v, &n);
  return p && !memchr(p, 0, n) ? p : NULL;
}
static int moc_mune_fields_(mulib_mune_state_t *s, mulib_mune_value_t v,
                            mulib_cdd_opencode_param_t *fields, size_t *count)
{
  size_t cursor = 0;
  int rc;
  mulib_mune_value_t key, value;
  if (v.type == MULIB_MUNE_NIL) return 1;
  if (v.type != MULIB_MUNE_TABLE) return 0;
  while ((rc = mulib_mune_table_next(s, v, &cursor, &key, &value)) == 1) {
    if (*count >= moc_fields_ || !(fields[*count].name = moc_mune_string_(key)) ||
        !(fields[*count].value = moc_mune_string_(value))) return 0;
    ++*count;
  }
  return rc == 0;
}
static mulib_mune_status_t moc_mune_call_(mulib_mune_state_t *s, const mulib_mune_value_t *args,
                                        size_t n, mulib_mune_result_t *out, void *user)
{
  mulib_cdd_opencode_param_t path[moc_fields_], query[moc_fields_];
  mulib_cdd_opencode_request_t q;
  mulib_cdd_opencode_response_t r;
  const mulib_cdd_opencode_operation_t *op;
  mulib_mune_value_t body_value = mulib_mune_nil();
  size_t status = 0;
  int rc = MULIB_CDD_OPENCODE_INVALID;
  memset(&q, 0, sizeof q);
  memset(&r, 0, sizeof r);
  q.path = path; q.query = query;
  if (n && n <= 3 && (q.operation_id = moc_mune_string_(args[0])) &&
      (n < 2 || moc_mune_fields_(s, args[1], path, &q.path_count)) &&
      (n < 3 || moc_mune_fields_(s, args[2], query, &q.query_count))) {
    op = mulib_cdd_opencode_operation(q.operation_id);
    if (!op || strcmp(op->method, "GET") || (op->flags & 2u)) rc = MULIB_CDD_OPENCODE_UNSUPPORTED;
    else rc = mulib_cdd_opencode_request((const mulib_cdd_opencode_client_t *)user, &q, &r);
  }
  if (rc == MULIB_CDD_OPENCODE_OK || rc == MULIB_CDD_OPENCODE_HTTP_ERROR) {
    status = r.http.message.status;
    if (mulib_mune_make_string(s, r.http.message.body ? (const char *)r.http.message.body : "",
                               r.http.message.body_length, &body_value) != MULIB_MUNE_OK) {
      mulib_cdd_opencode_response_free(&r);
      return MULIB_MUNE_MEMORY;
    }
  }
  mulib_cdd_opencode_response_free(&r);
  return mulib_mune_result_push(out, body_value) &&
         mulib_mune_result_push(out, status ? mulib_mune_integer((int64_t)status) : mulib_mune_nil()) &&
         mulib_mune_result_push(out, mulib_mune_integer(rc)) ? MULIB_MUNE_OK : MULIB_MUNE_MEMORY;
}
mulib_mune_status_t mulib_cdd_opencode_bind(mulib_mune_state_t *s,
                                           const mulib_cdd_opencode_client_t *c)
{
  size_t n;
  if (!s || !c || !moc_origin_(c, &n)) return MULIB_MUNE_INVALID;
  return mulib_mune_bind(s, "opencode_request", moc_mune_call_, (void *)c);
}
#ifdef __cplusplus
}
#endif
#endif
