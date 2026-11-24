/** @file mulib-openai.h
 * @brief Shared OpenAI REST catalog, routing, JSON and protocol utilities.
 * Define MULIB_IMPL_OPENAI; dependencies JSON, HTTP and their closures. */
#ifndef MULIB_OPENAI_H
#define MULIB_OPENAI_H
#include "mulib-http.h"
#include "mulib-json.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum mulib_openai_result {
  MULIB_OPENAI_OK = 1, MULIB_OPENAI_INVALID = -1, MULIB_OPENAI_LIMIT = -2,
  MULIB_OPENAI_NOMEM = -3, MULIB_OPENAI_TRANSPORT = -4,
  MULIB_OPENAI_CANCELLED = -7, MULIB_OPENAI_API_ERROR = -8,
  MULIB_OPENAI_PROTOCOL = -9, MULIB_OPENAI_NOT_FOUND = -10
} mulib_openai_result;
typedef struct mulib_openai_operation {
  const char *id, *method, *path, *content_types;
  int body_required;
} mulib_openai_operation_t;
/** Immutable catalog generated from standards/openai-openapi/openapi.json. */
size_t mulib_openai_operation_count (void);
const mulib_openai_operation_t *mulib_openai_operation_at (size_t);
const mulib_openai_operation_t *mulib_openai_operation (const char *id);
/** Match origin-form target relative to API base (e.g. /responses/x?foo=bar).
 * NULL method matches any method (for distinguishing 404/405). Literal routes
 * and fixed query predicates take precedence over parameterized routes. */
const mulib_openai_operation_t *mulib_openai_match (const char *method, const char *target);
/** Build malloc-owned relative target. Path parameters are encoded as single
 * segments; query fields use percent encoding and may repeat. Zero limit=8192.
 * Missing/extra/duplicate path parameters are invalid. */
int mulib_openai_target (const mulib_openai_operation_t *,
    const mulib_http_header_t *path, size_t path_count,
    const mulib_http_header_t *query, size_t query_count, size_t limit, char **out);
/** Decode a named path parameter or first query value into malloc-owned text.
 * Reject malformed escapes, decoded NUL/control bytes and invalid UTF-8.
 * NOT_FOUND when absent; output unchanged on all errors. */
int mulib_openai_path_parameter (const mulib_openai_operation_t *, const char *target,
                                 const char *name, char **out);
int mulib_openai_query (const char *target, const char *name, char **out);
int mulib_openai_media_type (const char *value, const char *type);
int mulib_openai_accepts (const mulib_openai_operation_t *, const char *content_type);
/** Independent JSON copy; NULL on invalid input/allocation failure. */
mulib_json_t *mulib_openai_clone (const mulib_json_t *);
/** Standard {error:{message,type,param,code}} envelope; nullable param/code. */
mulib_json_t *mulib_openai_error (const char *message, const char *type,
                                const char *param, const char *code);
/** Concatenate Responses output_text blocks or first Chat/Completion choice.
 * Returned JSON string preserves embedded NULs; NULL if no text/OOM. */
mulib_json_t *mulib_openai_output_text (const mulib_json_t *response);
const char *mulib_openai_strerror (int);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OPENAI) && !defined(MULIB_OPENAI_IMPLEMENTED)
#define MULIB_OPENAI_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "mulib-utf8.h"
static const mulib_openai_operation_t mulib_openai_operations_[] = {
#include "detail/mulib-openai-operations.inc"
};
size_t mulib_openai_operation_count (void)
{ return sizeof mulib_openai_operations_ / sizeof mulib_openai_operations_[0]; }
const mulib_openai_operation_t *mulib_openai_operation_at (size_t i)
{ return i < mulib_openai_operation_count () ? mulib_openai_operations_ + i : NULL; }
const mulib_openai_operation_t *mulib_openai_operation (const char *id)
{
  if (id) for (size_t i = 0; i < mulib_openai_operation_count (); ++i)
    if (!strcmp (id, mulib_openai_operations_[i].id)) return mulib_openai_operations_ + i;
  return NULL;
}
static int mulib_openai_hex_ (unsigned char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
static int mulib_openai_decode_ (const char *p, size_t n, int query, char **out)
{
  char *s;
  size_t k = 0;
  if (n == SIZE_MAX) return -2;
  s = (char *)malloc (n + 1);
  if (!s) return -3;
  for (size_t i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)p[i];
    if (c == '%') {
      int a, b;
      if (n - i < 3 || (a = mulib_openai_hex_ ((unsigned char)p[i + 1])) < 0
          || (b = mulib_openai_hex_ ((unsigned char)p[i + 2])) < 0) { free (s); return -1; }
      c = (unsigned char)(a * 16 + b); i += 2;
    } else if (query && c == '+') c = ' ';
    if (c < 32 || c == 127) { free (s); return -1; }
    s[k++] = (char)c;
  }
  if (!mulib_utf8_validate ((const unsigned char *)s, k)) { free (s); return -1; }
  s[k] = 0; *out = s; return 1;
}
int mulib_openai_query (const char *target, const char *name, char **out)
{
  const char *p;
  if (!target || !name || !out) return -1;
  p = strchr (target, '?');
  if (!p) return -10;
  ++p;
  while (*p) {
    const char *end = strchr (p, '&'), *equal;
    char *key = NULL;
    size_t n = end ? (size_t)(end - p) : strlen (p);
    int rc;
    equal = (const char *)memchr (p, '=', n);
    rc = mulib_openai_decode_ (p, equal ? (size_t)(equal - p) : n, 1, &key);
    if (rc != 1) return rc;
    rc = !strcmp (key, name); free (key);
    if (rc) return mulib_openai_decode_ (equal ? equal + 1 : p + n,
        equal ? n - (size_t)(equal - p) - 1 : 0, 1, out);
    if (!end) break;
    p = end + 1;
  }
  return -10;
}
static int mulib_openai_query_equal_ (const char *a, size_t n, const char *b, size_t m)
{
  size_t i = 0, j = 0;
  while (i < n && j < m) {
    unsigned char x = (unsigned char)a[i++], y = (unsigned char)b[j++];
    if (x == '%') {
      int h, l;
      if (n - i < 2 || (h = mulib_openai_hex_ ((unsigned char)a[i])) < 0
          || (l = mulib_openai_hex_ ((unsigned char)a[i + 1])) < 0) return 0;
      x = (unsigned char)(h * 16 + l); i += 2;
    } else if (x == '+') x = ' ';
    if (y == '%') {
      int h, l;
      if (m - j < 2 || (h = mulib_openai_hex_ ((unsigned char)b[j])) < 0
          || (l = mulib_openai_hex_ ((unsigned char)b[j + 1])) < 0) return 0;
      y = (unsigned char)(h * 16 + l); j += 2;
    } else if (y == '+') y = ' ';
    if (x != y) return 0;
  }
  return i == n && j == m;
}
static int mulib_openai_predicate_ (const char *target, const char *key, size_t kn,
    const char *value, size_t vn)
{
  const char *p = strchr (target, '?');
  if (!p) return 0;
  ++p;
  while (*p) {
    const char *end = strchr (p, '&'), *equal;
    size_t n = end ? (size_t)(end - p) : strlen (p);
    equal = (const char *)memchr (p, '=', n);
    if (mulib_openai_query_equal_ (p, equal ? (size_t)(equal - p) : n, key, kn))
      return mulib_openai_query_equal_ (equal ? equal + 1 : p + n,
          equal ? n - (size_t)(equal - p) - 1 : 0, value, vn);
    if (!end) break;
    p = end + 1;
  }
  return 0;
}
static int mulib_openai_route_ (const char *pattern, const char *target, size_t *score)
{
  size_t i = 0, j = 0, points = 0;
  while (pattern[i] && pattern[i] != '?') {
    if (pattern[i] == '{') {
      size_t start = j;
      while (pattern[i] && pattern[i] != '}') ++i;
      if (!pattern[i]) return 0;
      ++i;
      while (target[j] && target[j] != '/' && target[j] != '?') ++j;
      if (start == j) return 0;
    } else {
      if (pattern[i++] != target[j++]) return 0;
      ++points;
    }
  }
  if (target[j] && target[j] != '?') return 0;
  if (pattern[i] == '?') {
    /* Publication currently has a fixed beta=true query predicate. General
     * key/value predicates are supported so regeneration remains data-only. */
    const char *p = pattern + i + 1;
    while (*p) {
      const char *end = strchr (p, '&'), *equal;
      size_t n = end ? (size_t)(end - p) : strlen (p);
      equal = (const char *)memchr (p, '=', n);
      if (!equal) return 0;
      if (!mulib_openai_predicate_ (target, p, (size_t)(equal - p), equal + 1,
          n - (size_t)(equal - p) - 1)) return 0;
      points += 8192;
      if (!end) break;
      p = end + 1;
    }
  }
  *score = points; return 1;
}
const mulib_openai_operation_t *mulib_openai_match (const char *method, const char *target)
{
  const mulib_openai_operation_t *best = NULL;
  size_t score = 0;
  if (!target || target[0] != '/' || target[1] == '/' || strchr (target, '#')) return NULL;
  for (size_t i = 0; i < mulib_openai_operation_count (); ++i) {
    const mulib_openai_operation_t *o = mulib_openai_operations_ + i;
    size_t s;
    if ((!method || !strcmp (method, o->method)) && mulib_openai_route_ (o->path, target, &s)
        && (!best || s > score)) { best = o; score = s; }
  }
  return best;
}
static int mulib_openai_append_ (mulib_strbuf_t *b, const char *p, size_t n, size_t limit)
{
  if (n > limit - b->length) return -2;
  return mulib_strbuf_append_n (b, p, n) ? 1 : -3;
}
static int mulib_openai_encode_ (mulib_strbuf_t *b, const char *p, size_t limit)
{
  static const char hex[] = "0123456789ABCDEF";
  if (!p || !mulib_utf8_validate ((const unsigned char *)p, strlen (p))) return -1;
  for (; *p; ++p) {
    unsigned char c = (unsigned char)*p;
    char q[3]; size_t n = 1;
    if (c < 32 || c == 127) return -1;
    q[0] = (char)c;
    /* Encode dots too: a path parameter may never introduce a dot segment. */
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
        || c == '-' || c == '_' || c == '~')) {
      q[0] = '%'; q[1] = hex[c >> 4]; q[2] = hex[c & 15]; n = 3;
    }
    { int rc = mulib_openai_append_ (b, q, n, limit); if (rc != 1) return rc; }
  }
  return 1;
}
int mulib_openai_target (const mulib_openai_operation_t *o,
    const mulib_http_header_t *path, size_t count, const mulib_http_header_t *query,
    size_t qcount, size_t limit, char **out)
{
  mulib_strbuf_t b = {0};
  size_t used = 0;
  int rc = -1, has_query;
  const char *p;
  if (!o || !o->path || !out || (!path && count) || (!query && qcount)) return -1;
  if (!limit) limit = 8192;
  if (count > limit || qcount > limit) return -2;
  for (size_t i = 0; i < count; ++i) {
    if (!path[i].name || !path[i].value || !*path[i].value) return -1;
    for (size_t j = 0; j < i; ++j) if (!strcmp (path[i].name, path[j].name)) return -1;
  }
  for (p = o->path; *p;) {
    if (*p == '{') {
      const char *end = strchr (++p, '}'), *value = NULL;
      if (!end) goto done;
      for (size_t i = 0; i < count; ++i)
        if (strlen (path[i].name) == (size_t)(end - p) && !memcmp (path[i].name, p, (size_t)(end - p))) value = path[i].value;
      if (!value) { rc = -1; goto done; }
      rc = mulib_openai_encode_ (&b, value, limit); if (rc != 1) goto done;
      ++used; p = end + 1;
    } else { rc = mulib_openai_append_ (&b, p++, 1, limit); if (rc != 1) goto done; }
  }
  if (used != count) { rc = -1; goto done; }
  has_query = strchr (o->path, '?') != NULL;
  for (size_t i = 0; i < qcount; ++i) {
    char *fixed = NULL;
    int fixed_rc;
    if (!query[i].name || !*query[i].name || !query[i].value) { rc = -1; goto done; }
    fixed_rc = mulib_openai_query (o->path, query[i].name, &fixed);
    if (fixed_rc != MULIB_OPENAI_NOT_FOUND) { free (fixed); rc = fixed_rc == 1 ? -1 : fixed_rc; goto done; }
    rc = mulib_openai_append_ (&b, has_query ? "&" : "?", 1, limit); if (rc != 1) goto done;
    has_query = 1;
    rc = mulib_openai_encode_ (&b, query[i].name, limit); if (rc != 1) goto done;
    rc = mulib_openai_append_ (&b, "=", 1, limit); if (rc != 1) goto done;
    rc = mulib_openai_encode_ (&b, query[i].value, limit); if (rc != 1) goto done;
  }
  *out = b.data; return 1;
done:
  mulib_strbuf_free (&b); return rc;
}
int mulib_openai_path_parameter (const mulib_openai_operation_t *o, const char *target,
    const char *name, char **out)
{
  size_t i = 0, j = 0, score;
  if (!o || !o->path || !target || !name || !out || !mulib_openai_route_ (o->path, target, &score)) return -1;
  while (o->path[i] && o->path[i] != '?') {
    if (o->path[i] == '{') {
      size_t key = ++i, start = j;
      while (o->path[i] != '}') ++i;
      while (target[j] && target[j] != '/' && target[j] != '?') ++j;
      if (strlen (name) == i - key && !memcmp (name, o->path + key, i - key))
        return mulib_openai_decode_ (target + start, j - start, 0, out);
      ++i;
    } else { ++i; ++j; }
  }
  return -10;
}
int mulib_openai_media_type (const char *value, const char *type)
{
  size_t n;
  if (!value || !type) return 0;
  n = strlen (type);
  for (size_t i = 0; i < n; ++i) {
    unsigned char c = (unsigned char)value[i], d = (unsigned char)type[i];
    if (!c) return 0;
    if (c >= 'A' && c <= 'Z') c += 32;
    if (d >= 'A' && d <= 'Z') d += 32;
    if (c != d) return 0;
  }
  value += n; while (*value == ' ' || *value == '\t') ++value;
  return !*value || *value == ';';
}
int mulib_openai_accepts (const mulib_openai_operation_t *o, const char *type)
{
  const char *p;
  if (!o || !o->content_types || !type) return 0;
  p = o->content_types;
  while (*p) {
    char media[128];
    const char *end = strchr (p, '|');
    size_t n = end ? (size_t)(end - p) : strlen (p);
    if (n < sizeof media) { memcpy (media, p, n); media[n] = 0;
      if (mulib_openai_media_type (type, media)) return 1; }
    if (!end) break;
    p = end + 1;
  }
  return 0;
}
mulib_json_t *mulib_openai_clone (const mulib_json_t *v)
{
  char *text;
  mulib_json_t *out;
  if (!v || !(text = mulib_json_stringify (v, 0))) return NULL;
  out = mulib_json_parse (text, strlen (text), NULL); free (text); return out;
}
static int mulib_openai_set_ (mulib_json_t *o, const char *key, mulib_json_t *v)
{
  if (v && mulib_json_set (o, key, v)) return 1;
  mulib_json_destroy (v); return 0;
}
mulib_json_t *mulib_openai_error (const char *message, const char *type, const char *param, const char *code)
{
  mulib_json_t *out = NULL, *e;
  if (!message || !type) return NULL;
  e = mulib_json_new (MULIB_JSON_OBJECT);
  if (!e) return NULL;
  if (!mulib_openai_set_ (e, "message", mulib_json_string (message))
      || !mulib_openai_set_ (e, "type", mulib_json_string (type))
      || !mulib_openai_set_ (e, "param", param ? mulib_json_string (param) : mulib_json_new (MULIB_JSON_NULL))
      || !mulib_openai_set_ (e, "code", code ? mulib_json_string (code) : mulib_json_new (MULIB_JSON_NULL))) goto fail;
  out = mulib_json_new (MULIB_JSON_OBJECT);
  if (out && mulib_json_set (out, "error", e)) return out;
fail:
  mulib_json_destroy (out); mulib_json_destroy (e); return NULL;
}
mulib_json_t *mulib_openai_output_text (const mulib_json_t *r)
{
  mulib_strbuf_t b = {0};
  mulib_json_t *output = mulib_json_get (r, "output"), *out;
  int found = 0;
  for (size_t i = 0; i < mulib_json_size (output); ++i) {
    mulib_json_t *content = mulib_json_get (mulib_json_at (output, i), "content");
    for (size_t j = 0; j < mulib_json_size (content); ++j) {
      mulib_json_t *item = mulib_json_at (content, j);
      const char *type = mulib_json_text (mulib_json_get (item, "type"), NULL), *text;
      size_t n;
      text = mulib_json_text (mulib_json_get (item, "text"), &n);
      if (type && !strcmp (type, "output_text") && text) {
        found = 1; if (!mulib_strbuf_append_n (&b, text, n)) goto fail;
      }
    }
  }
  if (!found) {
    mulib_json_t *choice = mulib_json_at (mulib_json_get (r, "choices"), 0);
    mulib_json_t *text = mulib_json_get (mulib_json_get (choice, "message"), "content");
    if (!text) text = mulib_json_get (choice, "text");
    if (mulib_json_typeof (text) == MULIB_JSON_STRING) return mulib_openai_clone (text);
    return NULL;
  }
  out = mulib_json_string_n (b.data, b.length); mulib_strbuf_free (&b); return out;
fail:
  mulib_strbuf_free (&b); return NULL;
}
const char *mulib_openai_strerror (int rc)
{
  switch (rc) {
  case 1: return "success"; case -1: return "invalid argument or framing";
  case -2: return "resource limit"; case -3: return "allocation failed";
  case -4: return "transport failure"; case -7: return "callback cancelled";
  case -8: return "HTTP API error"; case -9: return "invalid OpenAI response";
  case -10: return "operation or parameter not found"; default: return "transport/protocol error";
  }
}
#endif
