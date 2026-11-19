/** @file mulib-graphql.h
 * @brief GraphQL-over-HTTP JSON POST client. Define MULIB_IMPL_CDD_GRAPHQL
 * in one C11 translation unit; see manual/mulib-graphql.md.
 */
#ifndef MULIB_GRAPHQL_H
#define MULIB_GRAPHQL_H
#include "mulib-rest.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_graphql_result {
  mulib_rest_response_t response; /* owned HTTP + parsed GraphQL document */
  int has_errors;                /* valid nonempty top-level errors array */
} mulib_graphql_result_t;
typedef mulib_graphql_result_t mulib_cdd_graphql_result_t;

/** variables and operation_name are optional; variables must be an object.
 * HTTP and GraphQL errors are completed exchanges if the envelope is valid;
 * inspect status and has_errors. Return 1 or a negative error (-1 invalid,
 * -2 local construction error, or REST/MuW3 status). On failure out is
 * unchanged; it must not already own a result on success. */
int mulib_cdd_graphql_execute(const mulib_muw3_client_t *client, const char *url,
    const char *query, const mulib_json_t *variables, const char *operation_name,
    const mulib_http_header_t *headers, size_t header_count,
    mulib_cdd_graphql_result_t *out);
void mulib_cdd_graphql_result_free(mulib_cdd_graphql_result_t *out);

/* Compatibility declarations; MULIB_IMPL_GRAPHQL is also supported. */
int mulib_graphql_execute(const mulib_muw3_client_t *client, const char *url,
    const char *query, const mulib_json_t *variables, const char *operation_name,
    const mulib_http_header_t *headers, size_t header_count,
    mulib_graphql_result_t *out);
void mulib_graphql_result_free(mulib_graphql_result_t *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_CDD_GRAPHQL) || defined(MULIB_IMPL_GRAPHQL)
#ifndef MULIB_GRAPHQL_IMPLEMENTED
#define MULIB_GRAPHQL_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int mulib_graphql_name_(const char *s) {
  unsigned char c = (unsigned char)*s;
  if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
    return 0;
  while ((c = (unsigned char)*++s) != 0) {
    if (!(c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9')))
      return 0;
  }
  return 1;
}

static int mulib_graphql_accept_(const char *s) {
  const char *name = "accept";
  while (*s && *name) {
    unsigned char x = (unsigned char)*s++;
    if (x >= 'A' && x <= 'Z') x += 'a' - 'A';
    if (x != (unsigned char)*name++) return 0;
  }
  return *s == 0 && *name == 0;
}

/* Takes ownership of v even when insertion fails. */
static int mulib_graphql_set_(mulib_json_t *obj, const char *key, mulib_json_t *v) {
  if (v && mulib_json_set(obj, key, v)) return 1;
  mulib_json_destroy(v);
  return 0;
}

static int mulib_graphql_index_(const mulib_json_t *v, double min) {
  double n, integer;
  if (mulib_json_typeof(v) != MULIB_JSON_NUMBER) return 0;
  n = mulib_json_numeric(v);
  return n >= min && modf(n, &integer) == 0.0;
}

static int mulib_graphql_error_(const mulib_json_t *v) {
  const mulib_json_t *part;
  if (mulib_json_typeof(v) != MULIB_JSON_OBJECT ||
      mulib_json_typeof(mulib_json_get(v, "message")) != MULIB_JSON_STRING)
    return 0;
  part = mulib_json_get(v, "locations");
  if (part) {
    if (mulib_json_typeof(part) != MULIB_JSON_ARRAY) return 0;
    for (size_t i = 0; i < mulib_json_size(part); ++i) {
      const mulib_json_t *loc = mulib_json_at(part, i);
      if (mulib_json_typeof(loc) != MULIB_JSON_OBJECT ||
          !mulib_graphql_index_(mulib_json_get(loc, "line"), 1) ||
          !mulib_graphql_index_(mulib_json_get(loc, "column"), 1)) return 0;
    }
  }
  part = mulib_json_get(v, "path");
  if (part) {
    if (mulib_json_typeof(part) != MULIB_JSON_ARRAY || !mulib_json_size(part)) return 0;
    for (size_t i = 0; i < mulib_json_size(part); ++i) {
      const mulib_json_t *segment = mulib_json_at(part, i);
      if (mulib_json_typeof(segment) != MULIB_JSON_STRING &&
          !mulib_graphql_index_(segment, 0)) return 0;
    }
  }
  part = mulib_json_get(v, "extensions");
  return !part || mulib_json_typeof(part) == MULIB_JSON_OBJECT;
}

static int mulib_graphql_document_(const mulib_json_t *doc, int *has_errors) {
  const mulib_json_t *data, *errors, *extensions;
  if (mulib_json_typeof(doc) != MULIB_JSON_OBJECT) return 0;
  data = mulib_json_get(doc, "data");
  errors = mulib_json_get(doc, "errors");
  extensions = mulib_json_get(doc, "extensions");
  if ((!data && !errors) ||
      (data && mulib_json_typeof(data) != MULIB_JSON_OBJECT &&
       (mulib_json_typeof(data) != MULIB_JSON_NULL || !errors)) ||
      (extensions && mulib_json_typeof(extensions) != MULIB_JSON_OBJECT) ||
      mulib_json_size(doc) != (size_t)((data != NULL) + (errors != NULL) + (extensions != NULL)))
    return 0;
  if (errors) {
    if (mulib_json_typeof(errors) != MULIB_JSON_ARRAY || !mulib_json_size(errors)) return 0;
    for (size_t i = 0; i < mulib_json_size(errors); ++i)
      if (!mulib_graphql_error_(mulib_json_at(errors, i))) return 0;
  }
  *has_errors = errors != NULL;
  return 1;
}

void mulib_cdd_graphql_result_free(mulib_cdd_graphql_result_t *r) {
  if (r) {
    mulib_cdd_rest_response_free(&r->response);
    r->has_errors = 0;
  }
}

int mulib_cdd_graphql_execute(const mulib_muw3_client_t *client, const char *url,
    const char *query, const mulib_json_t *variables, const char *operation,
    const mulib_http_header_t *headers, size_t n, mulib_cdd_graphql_result_t *out) {
  mulib_json_t *object, *copy;
  mulib_http_header_t *h = NULL;
  mulib_cdd_graphql_result_t tmp = {0};
  int rc = -2, accept = 0;
  if (!out || !url || !*url || !query || !*query ||
      (variables && mulib_json_typeof(variables) != MULIB_JSON_OBJECT) ||
      (operation && !mulib_graphql_name_(operation)) || (!headers && n) || n > 254)
    return -1;
  for (size_t i = 0; i < n; ++i) {
    if (!headers[i].name || !headers[i].value) return -1;
    if (mulib_graphql_accept_(headers[i].name)) accept = 1;
  }
  if (!accept && n == 254) return -1;
  object = mulib_json_new(MULIB_JSON_OBJECT);
  if (!object) return -2;
  if (!mulib_graphql_set_(object, "query", mulib_json_string(query))) goto done;
  if (variables) {
    char *s = mulib_json_stringify(variables, 0);
    if (!s) goto done;
    copy = mulib_json_parse(s, strlen(s), NULL);
    free(s);
    if (!mulib_graphql_set_(object, "variables", copy)) goto done;
  }
  if (operation && !mulib_graphql_set_(object, "operationName", mulib_json_string(operation)))
    goto done;
  if (!accept) {
    h = (mulib_http_header_t *)malloc((n + 1) * sizeof(*h));
    if (!h) goto done;
    if (n) memcpy(h, headers, n * sizeof(*h));
    h[n].name = "Accept";
    h[n].value = "application/graphql-response+json, application/json;q=0.9";
    headers = h;
    ++n;
  }
  rc = mulib_cdd_rest_request(client, "POST", url, headers, n, object, 1, &tmp.response);
  if (rc == 1) {
    if (!mulib_graphql_document_(tmp.response.json, &tmp.has_errors)) {
      mulib_cdd_graphql_result_free(&tmp);
      rc = -1;
    } else *out = tmp;
  }
done:
  free(h);
  mulib_json_destroy(object);
  return rc;
}

int mulib_graphql_execute(const mulib_muw3_client_t *client, const char *url,
    const char *query, const mulib_json_t *variables, const char *operation,
    const mulib_http_header_t *headers, size_t n, mulib_graphql_result_t *out) {
  return mulib_cdd_graphql_execute(client, url, query, variables, operation, headers, n, out);
}

void mulib_graphql_result_free(mulib_graphql_result_t *r) {
  mulib_cdd_graphql_result_free(r);
}
#endif
#endif
