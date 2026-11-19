/** @file mulib-sse-client.h
 * @brief One synchronous EventSource-style GET over MuW3, no reconnection.
 */
#ifndef MULIB_SSE_CLIENT_H
#define MULIB_SSE_CLIENT_H
#include "mulib-muw3.h"
#include "mulib-sse.h"
#include "mulib-utf8.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mulib_sse_client_options {
  const char *last_event_id; /* borrowed UTF-8 header value, up to 8192 bytes */
  size_t max_event_bytes;   /* zero: 1 MiB per line/event after UTF-8 decoding */
} mulib_cdd_sse_client_options_t;
typedef mulib_cdd_sse_client_options_t mulib_sse_client_options_t;

enum {
  MULIB_CDD_SSE_CLIENT_STOP = -7,
  MULIB_CDD_SSE_CLIENT_REPRESENTATION = -8,
  MULIB_CDD_SSE_CLIENT_EVENT = -9
};

/** One GET; only 200 responses with a single text/event-stream Content-Type
 * and identity encoding reach on_event. Non-200 responses are returned without
 * events. No redirects. 1 transfers response ownership to out (free with
 * mulib_muw3_response_free); all failures leave out unchanged. Out must not
 * already own a response. Return exactly 1 from on_event to continue; any
 * other value stops. Event strings are borrowed only for the callback's
 * duration. Delivery is live, before HTTP framing is fully validated.
 * Returns the MuW3 result, SSE STOP, REPRESENTATION or EVENT, or -2/-3 for
 * event limit/allocation failures. MuW3 cumulative limits still apply.
 */
int mulib_cdd_sse_client_get(const mulib_muw3_client_t *client, const char *url,
    const mulib_cdd_sse_client_options_t *options, mulib_sse_event_fn on_event,
    void *user, mulib_muw3_response_t *out);

/** Legacy API: maps event parser failures/consumer stop to -9. */
int mulib_sse_client_get(const mulib_muw3_client_t *client, const char *url,
    const mulib_sse_client_options_t *options, mulib_sse_event_fn on_event,
    void *user, mulib_muw3_response_t *out);

#ifdef __cplusplus
}
#endif
#endif /* MULIB_SSE_CLIENT_H */

#if defined(MULIB_IMPL_CDD_SSE_CLIENT) || defined(MULIB_IMPL_SSE_CLIENT)
#ifndef MULIB_SSE_CLIENT_IMPLEMENTED
#define MULIB_SSE_CLIENT_IMPLEMENTED
#include <string.h>

typedef struct mulib_sse_client_ctx_ {
  mulib_sse_parser_t *parser;
  unsigned char sequence[4];
  unsigned used, need;
  int started, error, event_error;
} mulib_sse_client_ctx_;

static unsigned char mulib_sse_client_lower_(unsigned char c) {
  return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

static int mulib_sse_client_match_(const char **p, const char *name) {
  const char *s = *p;
  for (; *name; ++name, ++s)
    if (mulib_sse_client_lower_((unsigned char)*s) != (unsigned char)*name)
      return 0;
  *p = s;
  return 1;
}

static int mulib_sse_client_equal_(const char *s, const char *name) {
  return s && mulib_sse_client_match_(&s, name) && !*s;
}

static const char *mulib_sse_client_ows_(const char *s) {
  while (*s == ' ' || *s == '\t') ++s;
  return s;
}

static int mulib_sse_client_token_(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
      || (c >= '0' && c <= '9') || (c && strchr("!#$%&'*+-.^_`|~", c));
}

static int mulib_sse_client_mime_(const char *s) {
  if (!s) return 0;
  s = mulib_sse_client_ows_(s);
  if (!mulib_sse_client_match_(&s, "text/event-stream")) return 0;
  while (*(s = mulib_sse_client_ows_(s))) {
    if (*s++ != ';') return 0;
    s = mulib_sse_client_ows_(s);
    if (!*s || *s == ';') continue; /* optional empty parameter per HTTP */
    if (!mulib_sse_client_token_((unsigned char)*s)) return 0;
    do { ++s; } while (mulib_sse_client_token_((unsigned char)*s));
    if (*s++ != '=') return 0;
    if (*s == '"') {
      ++s;
      while (*s != '"') {
        unsigned char c;
        if (*s == '\\') ++s;
        c = (unsigned char)*s;
        if ((c < 32 && c != '\t') || c == 127) return 0;
        ++s;
      }
      ++s;
    } else {
      if (!mulib_sse_client_token_((unsigned char)*s)) return 0;
      do { ++s; } while (mulib_sse_client_token_((unsigned char)*s));
    }
  }
  return 1;
}

static int mulib_sse_client_headers_(const mulib_http_message_t *m) {
  int type = 0, encoding = 0;
  if (!m->headers && m->header_count) return 0;
  for (size_t i = 0; i < m->header_count; ++i) {
    const mulib_http_header_t *h = &m->headers[i];
    if (mulib_sse_client_equal_(h->name, "content-type")) {
      if (type || !mulib_sse_client_mime_(h->value)) return 0;
      type = 1;
    } else if (mulib_sse_client_equal_(h->name, "content-encoding")) {
      /* MuW3 unframes HTTP bodies but does not decompress them. */
      if (encoding || !mulib_sse_client_equal_(h->value, "identity")) return 0;
      encoding = 1;
    }
  }
  return type;
}

static int mulib_sse_client_parser_result_(mulib_sse_client_ctx_ *ctx, int rc) {
  if (rc == 1) return 1;
  ctx->event_error = 1;
  ctx->error = rc == -2 || rc == -3 || rc == -7 ? rc : MULIB_CDD_SSE_CLIENT_EVENT;
  return 0;
}

/* WHATWG UTF-8 decoder: replace ill-formed subsequences, reprocess a bad
 * continuation, and preserve valid scalars across arbitrary HTTP slices.
 * The existing SSE codec handles initial BOM and SSE line/event framing. */
static int mulib_sse_client_decode_(mulib_sse_client_ctx_ *ctx,
    const unsigned char *p, size_t n) {
  static const unsigned char replacement[] = {0xef, 0xbf, 0xbd};
  unsigned char buf[1024];
  size_t len = 0;
  while (n) {
    unsigned char c = *p;
    if (ctx->used) {
      unsigned char first = ctx->sequence[0];
      unsigned min = 0x80, max = 0xbf;
      if (ctx->used == 1) {
        if (first == 0xe0) min = 0xa0;
        if (first == 0xed) max = 0x9f;
        if (first == 0xf0) min = 0x90;
        if (first == 0xf4) max = 0x8f;
      }
      if (c < min || c > max) {
        memcpy(buf + len, replacement, sizeof replacement);
        len += sizeof replacement;
        ctx->used = 0;
      } else {
        ctx->sequence[ctx->used++] = c;
        if (ctx->used == ctx->need) {
          memcpy(buf + len, ctx->sequence, ctx->used);
          len += ctx->used;
          ctx->used = 0;
        }
        ++p;
        --n;
      }
    } else {
      if (c < 0x80) buf[len++] = c;
      else if (c >= 0xc2 && c <= 0xf4) {
        ctx->sequence[0] = c;
        ctx->used = 1;
        ctx->need = c < 0xe0 ? 2u : c < 0xf0 ? 3u : 4u;
      } else {
        memcpy(buf + len, replacement, sizeof replacement);
        len += sizeof replacement;
      }
      ++p;
      --n;
    }
    if (len > sizeof buf - 4) {
      if (!mulib_sse_client_parser_result_(ctx, mulib_sse_feed(ctx->parser, buf, len)))
        return 0;
      len = 0;
    }
  }
  return !len || mulib_sse_client_parser_result_(ctx, mulib_sse_feed(ctx->parser, buf, len));
}

static int mulib_sse_client_body_(void *user, const mulib_http_message_t *m,
    const void *body, size_t length) {
  mulib_sse_client_ctx_ *ctx = (mulib_sse_client_ctx_ *)user;
  if (ctx->error) return 0;
  if (!ctx->started) {
    ctx->started = 1;
    if (m->status == 200 && !mulib_sse_client_headers_(m)) {
      ctx->error = MULIB_CDD_SSE_CLIENT_REPRESENTATION;
      return 0;
    }
  }
  return m->status != 200 || !length
      || mulib_sse_client_decode_(ctx, (const unsigned char *)body, length);
}

static int mulib_sse_client_get_(const mulib_muw3_client_t *client, const char *url,
    const mulib_cdd_sse_client_options_t *opt, mulib_sse_event_fn handler,
    void *user, mulib_muw3_response_t *out, int legacy) {
  mulib_http_header_t headers[4] = {
    {(char *)"Accept", (char *)"text/event-stream"},
    {(char *)"Cache-Control", (char *)"no-cache"},
    {(char *)"Accept-Encoding", (char *)"identity"},
    {(char *)"Last-Event-ID", NULL}
  };
  mulib_muw3_request_t req = {0};
  mulib_muw3_response_t tmp = {0};
  mulib_sse_client_ctx_ ctx = {0};
  size_t count = 3;
  int rc;
  if (!url || !handler || !out) return -1;
  if (opt && opt->last_event_id && *opt->last_event_id) {
    const unsigned char *id = (const unsigned char *)opt->last_event_id;
    size_t n = 0;
    for (; n < 8192 && id[n]; ++n)
      if ((id[n] < 32 && id[n] != '\t') || id[n] == 127) return -1;
    if (id[n]) return -2;
    if (!mulib_utf8_validate(id, n)) return -1;
    headers[count++].value = (char *)opt->last_event_id;
  }
  ctx.parser = mulib_sse_create(opt ? opt->max_event_bytes : 0, handler, user);
  if (!ctx.parser) return -3;
  req.url = url;
  req.method = "GET";
  req.headers = headers;
  req.header_count = count;
  rc = (int)mulib_muw3_stream(client, &req, mulib_sse_client_body_, &ctx, &tmp);
  if (!ctx.error && rc == 1 && tmp.message.status == 200) {
    static const unsigned char replacement[] = {0xef, 0xbf, 0xbd};
    if (ctx.used)
      (void)mulib_sse_client_parser_result_(&ctx,
          mulib_sse_feed(ctx.parser, replacement, sizeof replacement));
    if (!ctx.error)
      (void)mulib_sse_client_parser_result_(&ctx, mulib_sse_finish(ctx.parser));
  }
  if (ctx.error) rc = legacy && ctx.event_error ? MULIB_CDD_SSE_CLIENT_EVENT : ctx.error;
  if (rc == 1) *out = tmp;
  else mulib_muw3_response_free(&tmp);
  mulib_sse_destroy(ctx.parser);
  return rc;
}

int mulib_cdd_sse_client_get(const mulib_muw3_client_t *client, const char *url,
    const mulib_cdd_sse_client_options_t *opt, mulib_sse_event_fn handler,
    void *user, mulib_muw3_response_t *out) {
  return mulib_sse_client_get_(client, url, opt, handler, user, out, 0);
}

int mulib_sse_client_get(const mulib_muw3_client_t *client, const char *url,
    const mulib_sse_client_options_t *opt, mulib_sse_event_fn handler,
    void *user, mulib_muw3_response_t *out) {
  return mulib_sse_client_get_(client, url, opt, handler, user, out, 1);
}
#endif
#endif
