/** @file mulib-grpc.h
 * @brief gRPC message framing and binary unary gRPC-Web over MuW3.
 * Define MULIB_IMPL_CDD_GRPC in one TU (legacy: MULIB_IMPL_GRPC).
 * Not native HTTP/2 gRPC. No Protobuf, compression, text mode or streaming.
 */
#ifndef MULIB_GRPC_H
#define MULIB_GRPC_H
#include "mulib-muw3.h"
#include "mulib-buffer.h"
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_grpc_frame {
  int compressed;              /* caller handles compression */
  const unsigned char *data;   /* borrows input */
  size_t length;
} mulib_cdd_grpc_frame_t;

/** 1 OK, 0 incomplete, -1 invalid, -2 length exceeds limit.
 * Both outputs are unchanged except on success; consumed may be NULL.
 * max_message==0 selects 16 MiB. NULL input is valid only for length==0. */
int mulib_cdd_grpc_frame_read(const void *data, size_t length, size_t max_message,
    mulib_cdd_grpc_frame_t *out, size_t *consumed);
/** Append one 5-byte-prefixed frame, or leave initialized out unchanged.
 * Returns 1 on success. Length must fit in a 32-bit wire field. */
int mulib_cdd_grpc_frame_write(mulib_buffer_t *out, int compressed,
    const void *data, size_t length);

typedef struct mulib_grpc_web_reply {
  mulib_muw3_response_t http;  /* owned for unary; empty for decode */
  unsigned char *message;     /* owned; NULL means no data frame */
  size_t message_length;
  int grpc_status;
  char *grpc_message;         /* owned optional percent-encoded ASCII text */
} mulib_cdd_grpc_web_reply_t;

/** Decode a complete parsed HTTP response without taking ownership of it.
 * Requires HTTP 200, binary gRPC-Web media type and identity encoding.
 * Requires final in-body trailers or an empty-body nonzero header status.
 * A successful RPC needs exactly one data frame; errors may omit it.
 * The metadata frame has a separate 16 KiB limit. max_message==0 means 16 MiB.
 * Returns 1 (including nonzero gRPC status), -1 invalid/unsupported,
 * -2 resource limit, -3 allocation failure. out unchanged on error;
 * on success free it before reuse. */
int mulib_cdd_grpc_web_decode(const mulib_http_message_t *response,
    size_t max_message, mulib_cdd_grpc_web_reply_t *out);
/** Binary unary HTTP/1 request. Same result and limit semantics as decode,
 * but also applies max_message to the request, returns MuW3 transport errors
 * unchanged, and retains the HTTP response on success. */
int mulib_cdd_grpc_web_unary(const mulib_muw3_client_t *client, const char *url,
    const void *request, size_t request_length, size_t max_message,
    mulib_cdd_grpc_web_reply_t *out);
void mulib_cdd_grpc_web_reply_free(mulib_cdd_grpc_web_reply_t *out);

/* Compatibility with the original protocol kernel. */
typedef mulib_cdd_grpc_frame_t mulib_grpc_frame_t;
typedef mulib_cdd_grpc_web_reply_t mulib_grpc_web_reply_t;
int mulib_grpc_frame_read(const void *, size_t, size_t, mulib_grpc_frame_t *, size_t *);
int mulib_grpc_frame_write(mulib_buffer_t *, int, const void *, size_t);
int mulib_grpc_web_unary(const mulib_muw3_client_t *, const char *, const void *,
    size_t, size_t, mulib_grpc_web_reply_t *);
void mulib_grpc_web_reply_free(mulib_grpc_web_reply_t *);
#ifdef __cplusplus
}
#endif
#endif /* declarations */

#if defined(MULIB_IMPL_CDD_GRPC) || defined(MULIB_IMPL_GRPC)
#ifndef MULIB_GRPC_IMPLEMENTED
#define MULIB_GRPC_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif

#define MULIB_CDD_GRPC_MESSAGE_LIMIT_ ((size_t)16777216)
#define MULIB_CDD_GRPC_TRAILER_LIMIT_ ((size_t)16384)

static uint32_t mulib_cdd_grpc_length_(const unsigned char *p)
{
  return ((uint32_t)p[1] << 24) | ((uint32_t)p[2] << 16) |
      ((uint32_t)p[3] << 8) | p[4];
}

int mulib_cdd_grpc_frame_read(const void *bytes, size_t n, size_t limit,
    mulib_cdd_grpc_frame_t *out, size_t *used)
{
  const unsigned char *p = (const unsigned char *)bytes;
  uint32_t len;
  if (!out || (!p && n)) return -1;
  if (n < 5) return 0;
  if (p[0] > 1) return -1;
  len = mulib_cdd_grpc_length_(p);
  if (len > (limit ? limit : MULIB_CDD_GRPC_MESSAGE_LIMIT_)) return -2;
  if (len > n - 5) return 0;
  out->compressed = (int)p[0];
  out->data = p + 5;
  out->length = (size_t)len;
  if (used) *used = (size_t)len + 5;
  return 1;
}

static void mulib_cdd_grpc_prefix_(unsigned char *p, int compressed, uint32_t n)
{
  p[0] = (unsigned char)compressed;
  p[1] = (unsigned char)(n >> 24);
  p[2] = (unsigned char)(n >> 16);
  p[3] = (unsigned char)(n >> 8);
  p[4] = (unsigned char)n;
}

int mulib_cdd_grpc_frame_write(mulib_buffer_t *out, int compressed,
    const void *data, size_t n)
{
  unsigned char *frame;
  int ok;
  if (!out || (compressed != 0 && compressed != 1) || (!data && n) ||
      n > UINT32_MAX || n > SIZE_MAX - 5 || out->length > SIZE_MAX - 5 - n)
    return 0;
  frame = (unsigned char *)malloc(5 + n);
  if (!frame) return 0;
  mulib_cdd_grpc_prefix_(frame, compressed, (uint32_t)n);
  if (n) memcpy(frame + 5, data, n); /* data may alias out->data */
  ok = mulib_buffer_append(out, frame, 5 + n);
  free(frame);
  return ok;
}

void mulib_cdd_grpc_web_reply_free(mulib_cdd_grpc_web_reply_t *r)
{
  if (!r) return;
  mulib_muw3_response_free(&r->http);
  free(r->message);
  free(r->grpc_message);
  memset(r, 0, sizeof(*r));
}

static int mulib_cdd_grpc_equal_(const char *s, size_t n, const char *lower)
{
  if (n != strlen(lower)) return 0;
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (c != (unsigned char)lower[i]) return 0;
  }
  return 1;
}

static int mulib_cdd_grpc_token_(unsigned char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') || (c && strchr("!#$%&'*+-.^_`|~", c));
}

static int mulib_cdd_grpc_ows_(char c) { return c == ' ' || c == '\t'; }

static int mulib_cdd_grpc_media_(const char *s)
{
  const char *start;
  size_t n;
  while (mulib_cdd_grpc_ows_(*s)) s++;
  start = s;
  while (*s == '/' || mulib_cdd_grpc_token_((unsigned char)*s)) s++;
  n = (size_t)(s - start);
  if (!mulib_cdd_grpc_equal_(start, n, "application/grpc-web+proto") &&
      !mulib_cdd_grpc_equal_(start, n, "application/grpc-web")) return 0;
  while (mulib_cdd_grpc_ows_(*s)) s++;
  while (*s == ';') {
    s++;
    while (mulib_cdd_grpc_ows_(*s)) s++;
    start = s;
    while (mulib_cdd_grpc_token_((unsigned char)*s)) s++;
    if (s == start || *s != '=') return 0;
    s++;
    if (*s == '"') {
      s++;
      while (*s != '"') {
        unsigned char c = (unsigned char)*s++;
        if (c == '\\') c = (unsigned char)*s++;
        if ((c < 32 && c != '\t') || c >= 127) return 0;
      }
      s++;
    } else {
      start = s;
      while (mulib_cdd_grpc_token_((unsigned char)*s)) s++;
      if (s == start) return 0;
    }
    while (mulib_cdd_grpc_ows_(*s)) s++;
  }
  return !*s;
}

static int mulib_cdd_grpc_identity_(const char *s)
{
  size_t n;
  while (mulib_cdd_grpc_ows_(*s)) s++;
  n = strlen(s);
  while (n && mulib_cdd_grpc_ows_(s[n - 1])) n--;
  return mulib_cdd_grpc_equal_(s, n, "identity");
}

static int mulib_cdd_grpc_status_(const char *s, size_t n, int *out)
{
  int code = 0;
  if (!n || n > 2 || (n > 1 && *s == '0')) return 0;
  for (size_t i = 0; i < n; i++) {
    if (s[i] < '0' || s[i] > '9') return 0;
    code = code * 10 + (s[i] - '0');
  }
  if (code > 16) return 0;
  *out = code;
  return 1;
}

static int mulib_cdd_grpc_hex_(unsigned char c)
{
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||
      (c >= 'a' && c <= 'f');
}

static int mulib_cdd_grpc_message_(const char *s, size_t n)
{
  for (size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c < 32 || c > 126) return 0;
    if (c == '%') {
      if (n - i < 3 || !mulib_cdd_grpc_hex_((unsigned char)s[i + 1]) ||
          !mulib_cdd_grpc_hex_((unsigned char)s[i + 2])) return 0;
      i += 2;
    }
  }
  return 1;
}

static int mulib_cdd_grpc_trailers_(const char *s, size_t n, int *status,
    const char **message, size_t *message_length)
{
  int found = 0;
  while (n) {
    size_t name = 0, start, end, next;
    while (name < n && mulib_cdd_grpc_token_((unsigned char)s[name])) name++;
    if (!name || name == n || s[name] != ':') return 0;
    start = end = name + 1;
    while (end < n && s[end] != '\r') {
      unsigned char c = (unsigned char)s[end++];
      if ((c < 32 && c != '\t') || c > 126) return 0;
    }
    next = end;
    if (next < n) {
      if (n - next < 2 || s[next + 1] != '\n') return 0;
      next += 2;
    }
    while (start < end && mulib_cdd_grpc_ows_(s[start])) start++;
    while (end > start && mulib_cdd_grpc_ows_(s[end - 1])) end--;
    if (mulib_cdd_grpc_equal_(s, name, "grpc-status")) {
      if (found || !mulib_cdd_grpc_status_(s + start, end - start, status)) return 0;
      found = 1;
    } else if (mulib_cdd_grpc_equal_(s, name, "grpc-message")) {
      if (*message || !mulib_cdd_grpc_message_(s + start, end - start)) return 0;
      *message = s + start;
      *message_length = end - start;
    } else if (mulib_cdd_grpc_equal_(s, name, "content-length") ||
               mulib_cdd_grpc_equal_(s, name, "transfer-encoding") ||
               mulib_cdd_grpc_equal_(s, name, "content-encoding") ||
               mulib_cdd_grpc_equal_(s, name, "content-type") ||
               mulib_cdd_grpc_equal_(s, name, "grpc-encoding") ||
               mulib_cdd_grpc_equal_(s, name, "host")) {
      return 0;
    }
    s += next;
    n -= next;
  }
  return found;
}

int mulib_cdd_grpc_web_decode(const mulib_http_message_t *r, size_t max_message,
    mulib_cdd_grpc_web_reply_t *out)
{
  const char *media = NULL, *encoding = NULL, *grpc_encoding = NULL;
  const char *status = NULL, *message = NULL;
  const unsigned char *data = NULL;
  size_t n, data_length = 0, message_length = 0;
  size_t limit = max_message ? max_message : MULIB_CDD_GRPC_MESSAGE_LIMIT_;
  mulib_cdd_grpc_web_reply_t tmp;
  memset(&tmp, 0, sizeof(tmp));
  if (!r || !out || !r->is_response || r->status != 200 ||
      (!r->headers && r->header_count) || (!r->trailers && r->trailer_count) ||
      (!r->body && r->body_length)) return -1;
  for (size_t i = 0; i < r->header_count; i++) {
    const char *name = r->headers[i].name, *value = r->headers[i].value;
    const char **slot = NULL;
    size_t len;
    if (!name || !value) return -1;
    len = strlen(name);
    if (mulib_cdd_grpc_equal_(name, len, "content-type")) slot = &media;
    else if (mulib_cdd_grpc_equal_(name, len, "content-encoding")) slot = &encoding;
    else if (mulib_cdd_grpc_equal_(name, len, "grpc-encoding")) slot = &grpc_encoding;
    else if (mulib_cdd_grpc_equal_(name, len, "grpc-status")) slot = &status;
    else if (mulib_cdd_grpc_equal_(name, len, "grpc-message")) slot = &message;
    if (slot) {
      if (*slot) return -1;
      *slot = value;
    }
  }
  if (!media || !mulib_cdd_grpc_media_(media) ||
      (encoding && !mulib_cdd_grpc_identity_(encoding)) ||
      (grpc_encoding && !mulib_cdd_grpc_identity_(grpc_encoding))) return -1;
  for (size_t i = 0; i < r->trailer_count; i++) {
    const char *name = r->trailers[i].name;
    size_t len;
    if (!name) return -1;
    len = strlen(name);
    if (mulib_cdd_grpc_equal_(name, len, "grpc-status") ||
        mulib_cdd_grpc_equal_(name, len, "grpc-message") ||
        mulib_cdd_grpc_equal_(name, len, "grpc-encoding")) return -1;
  }
  n = r->body_length;
  if (!n) {
    if (!status || !mulib_cdd_grpc_status_(status, strlen(status), &tmp.grpc_status) ||
        !tmp.grpc_status) return -1;
    if (message) {
      message_length = strlen(message);
      if (message_length > MULIB_CDD_GRPC_TRAILER_LIMIT_) return -2;
      if (!mulib_cdd_grpc_message_(message, message_length)) return -1;
    }
  } else {
    const unsigned char *p = r->body;
    int seen_trailers = 0;
    if (status || message) return -1;
    while (n) {
      uint32_t size;
      if (n < 5 || (p[0] != 0 && p[0] != 0x80)) return -1;
      size = mulib_cdd_grpc_length_(p);
      if (size > (p[0] ? MULIB_CDD_GRPC_TRAILER_LIMIT_ : limit)) return -2;
      if (size > n - 5) return -1;
      if (p[0] == 0) {
        if (data) return -1;
        data = p + 5;
        data_length = (size_t)size;
      } else {
        if (size != n - 5 || !mulib_cdd_grpc_trailers_((const char *)p + 5,
            (size_t)size, &tmp.grpc_status, &message, &message_length)) return -1;
        seen_trailers = 1;
      }
      p += (size_t)size + 5;
      n -= (size_t)size + 5;
    }
    if (!seen_trailers || (!tmp.grpc_status && !data)) return -1;
  }
  if (data) {
    tmp.message = (unsigned char *)malloc(data_length ? data_length : 1);
    if (!tmp.message) return -3;
    if (data_length) memcpy(tmp.message, data, data_length);
    tmp.message_length = data_length;
  }
  if (message) {
    tmp.grpc_message = (char *)malloc(message_length + 1);
    if (!tmp.grpc_message) {
      free(tmp.message);
      return -3;
    }
    memcpy(tmp.grpc_message, message, message_length);
    tmp.grpc_message[message_length] = 0;
  }
  *out = tmp;
  return 1;
}

int mulib_cdd_grpc_web_unary(const mulib_muw3_client_t *client, const char *url,
    const void *request, size_t n, size_t max_message, mulib_cdd_grpc_web_reply_t *out)
{
  mulib_muw3_response_t http;
  mulib_muw3_request_t q;
  mulib_muw3_client_t bounded;
  mulib_http_header_t headers[] = {
    {(char *)"Content-Type", (char *)"application/grpc-web+proto"},
    {(char *)"Accept", (char *)"application/grpc-web+proto"},
    {(char *)"X-Grpc-Web", (char *)"1"},
    {(char *)"Accept-Encoding", (char *)"identity"},
    {(char *)"Grpc-Accept-Encoding", (char *)"identity"}
  };
  unsigned char *frame;
  int rc;
  size_t limit = max_message ? max_message : MULIB_CDD_GRPC_MESSAGE_LIMIT_;
  size_t overhead = 10 + MULIB_CDD_GRPC_TRAILER_LIMIT_, http_limit;
  if (!out || !url || (!request && n)) return -1;
  if (n > limit || n > UINT32_MAX || n > SIZE_MAX - 5) return -2;
  bounded = client ? *client : mulib_muw3_client_default();
  http_limit = bounded.options.web.http.max_body_bytes;
  if (!http_limit) http_limit = MULIB_CDD_GRPC_MESSAGE_LIMIT_; /* HTTP default */
  if (limit <= SIZE_MAX - overhead && http_limit > limit + overhead) {
    http_limit = limit + overhead;
    bounded.options.web.http.max_body_bytes = http_limit;
  }
  if (n + 5 > http_limit) return -2;
  frame = (unsigned char *)malloc(n + 5);
  if (!frame) return -3;
  mulib_cdd_grpc_prefix_(frame, 0, (uint32_t)n);
  if (n) memcpy(frame + 5, request, n);
  memset(&http, 0, sizeof(http));
  q.url = url;
  q.method = "POST";
  q.body = frame;
  q.body_length = n + 5;
  q.headers = headers;
  q.header_count = sizeof(headers) / sizeof(*headers);
  rc = (int)mulib_muw3_perform(&bounded, &q, &http);
  free(frame);
  if (rc != 1) return rc;
  rc = mulib_cdd_grpc_web_decode(&http.message, max_message, out);
  if (rc == 1) out->http = http;
  else mulib_muw3_response_free(&http);
  return rc;
}

int mulib_grpc_frame_read(const void *data, size_t n, size_t limit,
    mulib_grpc_frame_t *out, size_t *consumed)
{ return mulib_cdd_grpc_frame_read(data, n, limit, out, consumed); }

int mulib_grpc_frame_write(mulib_buffer_t *out, int compressed, const void *data, size_t n)
{
  return n <= MULIB_CDD_GRPC_MESSAGE_LIMIT_ &&
      mulib_cdd_grpc_frame_write(out, compressed, data, n);
}

int mulib_grpc_web_unary(const mulib_muw3_client_t *client, const char *url,
    const void *request, size_t length, size_t limit, mulib_grpc_web_reply_t *out)
{ return mulib_cdd_grpc_web_unary(client, url, request, length, limit, out); }

void mulib_grpc_web_reply_free(mulib_grpc_web_reply_t *out)
{ mulib_cdd_grpc_web_reply_free(out); }

#undef MULIB_CDD_GRPC_MESSAGE_LIMIT_
#undef MULIB_CDD_GRPC_TRAILER_LIMIT_
#ifdef __cplusplus
}
#endif
#endif /* implementation guard */
#endif /* implementation switch */
