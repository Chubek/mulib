/** @file mulib-rest.h
 * @brief REST-oriented HTTP/HTTPS client built on MuW3. Define
 * MULIB_IMPL_CDD_REST in one C11 translation unit; see manual/mulib-rest.md.
 */
#ifndef MULIB_REST_H
#define MULIB_REST_H
#include "mulib-muw3.h"
#include "mulib-json.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_rest_response {
  mulib_muw3_response_t http; /* owned */
  mulib_json_t *json;        /* owned; NULL if empty or not parsed */
} mulib_rest_response_t;
typedef mulib_rest_response_t mulib_cdd_rest_response_t;

/** Return 1 for a completed HTTP request (including 4xx/5xx), -1 invalid,
 * -2 allocation/JSON encoding error, otherwise MuW3 error code.
 * NULL method means GET. NULL json_body sends no body. Default JSON headers
 * are added unless a case-insensitive match is already supplied. Parse the
 * entire nonempty response body if parse_json != 0. Failure leaves out
 * unchanged; out must not already own a response on success. */
int mulib_cdd_rest_request(const mulib_muw3_client_t *client, const char *method,
    const char *url, const mulib_http_header_t *headers, size_t header_count,
    const mulib_json_t *json_body, int parse_json, mulib_cdd_rest_response_t *out);
/** Append form-encoded pairs before any fragment in a valid URI reference.
 * max_bytes excludes NUL; zero selects 8192. Caller frees the returned string.
 * Returns NULL on error. */
char *mulib_cdd_rest_url_query(const char *url, const mulib_http_header_t *pairs,
    size_t count, size_t max_bytes);
void mulib_cdd_rest_response_free(mulib_cdd_rest_response_t *response);

/* Compatibility declarations; MULIB_IMPL_REST selects the same definitions. */
int mulib_rest_request(const mulib_muw3_client_t *client, const char *method,
    const char *url, const mulib_http_header_t *headers, size_t header_count,
    const mulib_json_t *json_body, int parse_json, mulib_rest_response_t *out);
char *mulib_rest_url_query(const char *url, const mulib_http_header_t *pairs,
    size_t count, size_t max_bytes);
void mulib_rest_response_free(mulib_rest_response_t *response);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_CDD_REST) || defined(MULIB_IMPL_REST)
#ifndef MULIB_REST_IMPLEMENTED
#define MULIB_REST_IMPLEMENTED
#include "mulib-uri.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int mulib_rest_header_eq_(const char *a, const char *b) {
  while (*a && *b) {
    unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
    if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
    if (y >= 'A' && y <= 'Z') y += 'a' - 'A';
    if (x != y) return 0;
  }
  return *a == 0 && *b == 0;
}

void mulib_cdd_rest_response_free(mulib_cdd_rest_response_t *r) {
  if (r) {
    mulib_muw3_response_free(&r->http);
    mulib_json_destroy(r->json);
    *r = (mulib_cdd_rest_response_t){0};
  }
}

char *mulib_cdd_rest_url_query(const char *url, const mulib_http_header_t *pairs,
    size_t count, size_t max_bytes) {
  char *form = NULL, *result, separator = 0;
  mulib_uri_t uri;
  size_t a = 0, b = 0, prefix, extra = 0, limit = max_bytes ? max_bytes : 8192;
  if (!url || (!pairs && count)) return NULL;
  if (limit == SIZE_MAX) --limit; /* Reserve one byte for the terminator. */
  while (a < limit && url[a]) ++a;
  if (!a || url[a] || !mulib_uri_parse(url, a, &uri)) return NULL;
  prefix = uri.has_fragment ? a - uri.fragment.length - 1 : a;
  if (count) {
    if (!uri.has_query) separator = '?';
    else if (uri.query.length && url[prefix - 1] != '&') separator = '&';
    extra = separator != 0;
    if (limit - a <= extra ||
        mulib_muw3_form(pairs, count, limit - a - extra, &form) != MULIB_MUW3_OK)
      return NULL;
    b = strlen(form);
  }
  result = (char *)malloc(a + extra + b + 1);
  if (result) {
    memcpy(result, url, prefix);
    if (extra) result[prefix] = separator;
    if (b) memcpy(result + prefix + extra, form, b);
    memcpy(result + prefix + extra + b, url + prefix, a - prefix + 1);
  }
  free(form);
  return result;
}

int mulib_cdd_rest_request(const mulib_muw3_client_t *client, const char *method,
    const char *url, const mulib_http_header_t *headers, size_t n,
    const mulib_json_t *body, int parse_json, mulib_cdd_rest_response_t *out) {
  mulib_http_header_t *h = NULL;
  mulib_cdd_rest_response_t tmp = {0};
  mulib_muw3_request_t q = {0};
  char *encoded = NULL;
  size_t count = n;
  int ct = 0, accept = 0, rc;
  if (!out || !url || !*url || (method && !*method) || (!headers && n) || n > 254)
    return -1;
  for (size_t i = 0; i < n; ++i) {
    if (!headers[i].name || !headers[i].value) return -1;
    if (mulib_rest_header_eq_(headers[i].name, "Content-Type")) ct = 1;
    if (mulib_rest_header_eq_(headers[i].name, "Accept")) accept = 1;
  }
  if (body) {
    encoded = mulib_json_stringify(body, 0);
    if (!encoded) return -2;
  }
  if ((body && !ct) || !accept) {
    h = (mulib_http_header_t *)malloc((n + (body && !ct) + !accept) * sizeof(*h));
    if (!h) { free(encoded); return -2; }
    if (n) memcpy(h, headers, n * sizeof(*h));
    if (body && !ct) {
      h[count].name = "Content-Type";
      h[count++].value = "application/json";
    }
    if (!accept) {
      h[count].name = "Accept";
      h[count++].value = "application/json";
    }
  }
  q.url = url;
  q.method = method ? method : "GET";
  q.headers = h ? h : headers;
  q.header_count = count;
  q.body = encoded;
  q.body_length = encoded ? strlen(encoded) : 0;
  rc = (int)mulib_muw3_perform(client, &q, &tmp.http);
  free(h);
  free(encoded);
  if (rc != MULIB_MUW3_OK) return rc;
  if (parse_json && tmp.http.message.body_length) {
    tmp.json = mulib_json_parse((const char *)tmp.http.message.body,
                                tmp.http.message.body_length, NULL);
    if (!tmp.json) { mulib_cdd_rest_response_free(&tmp); return -1; }
  }
  *out = tmp;
  return 1;
}

int mulib_rest_request(const mulib_muw3_client_t *client, const char *method,
    const char *url, const mulib_http_header_t *headers, size_t n,
    const mulib_json_t *body, int parse_json, mulib_rest_response_t *out) {
  return mulib_cdd_rest_request(client, method, url, headers, n, body, parse_json, out);
}

char *mulib_rest_url_query(const char *url, const mulib_http_header_t *pairs,
    size_t count, size_t max_bytes) {
  return mulib_cdd_rest_url_query(url, pairs, count, max_bytes);
}

void mulib_rest_response_free(mulib_rest_response_t *r) {
  mulib_cdd_rest_response_free(r);
}
#endif
#endif
