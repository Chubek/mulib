/** @file mulib-jsonrpc.h
 * @brief JSON-RPC 2.0 request/response validation over HTTP. Define
 * MULIB_IMPL_CDD_JSONRPC in one C11 translation unit; see manual/mulib-jsonrpc.md.
 */
#ifndef MULIB_JSONRPC_H
#define MULIB_JSONRPC_H
#include "mulib-rest.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_jsonrpc_reply {
  mulib_rest_response_t response; /* owned HTTP + parsed JSON */
  int remote_error;               /* at least one reply is an error */
} mulib_jsonrpc_reply_t;
typedef mulib_jsonrpc_reply_t mulib_cdd_jsonrpc_reply_t;

/** Build an owned JSON-RPC request. NULL id omits id (notification).
 * A non-NULL id must be string, number or JSON null; prefer string.
 * NULL params omits params; otherwise it must be an array or object.
 * Returns NULL on invalid input or construction failure. */
mulib_json_t *mulib_cdd_jsonrpc_make(const char *method, const mulib_json_t *params,
    const mulib_json_t *id);
/** Exchange a validated request object or batch of 1 to 1024 items by HTTP POST.
 * Success (1) requires matching response shape and IDs independent of order.
 * Duplicate IDs are rejected. Only notifications (or all-notification batches)
 * expect an empty body. Negative return leaves out unchanged; out must not
 * already own a reply on success. */
int mulib_cdd_jsonrpc_exchange(const mulib_muw3_client_t *client, const char *url,
    const mulib_json_t *request, const mulib_http_header_t *headers, size_t n,
    mulib_cdd_jsonrpc_reply_t *out);
/** Build and exchange a single request. NULL id sends a notification. */
int mulib_cdd_jsonrpc_call(const mulib_muw3_client_t *client, const char *url,
    const char *method, const mulib_json_t *params, const mulib_json_t *id,
    mulib_cdd_jsonrpc_reply_t *out);
void mulib_cdd_jsonrpc_reply_free(mulib_cdd_jsonrpc_reply_t *out);

/* Compatibility declarations; MULIB_IMPL_JSONRPC is also supported. */
mulib_json_t *mulib_jsonrpc_make(const char *method, const mulib_json_t *params,
    const mulib_json_t *id);
int mulib_jsonrpc_exchange(const mulib_muw3_client_t *client, const char *url,
    const mulib_json_t *request, const mulib_http_header_t *headers, size_t n,
    mulib_jsonrpc_reply_t *out);
int mulib_jsonrpc_call(const mulib_muw3_client_t *client, const char *url,
    const char *method, const mulib_json_t *params, const mulib_json_t *id,
    mulib_jsonrpc_reply_t *out);
void mulib_jsonrpc_reply_free(mulib_jsonrpc_reply_t *out);
#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_CDD_JSONRPC) || defined(MULIB_IMPL_JSONRPC)
#ifndef MULIB_JSONRPC_IMPLEMENTED
#define MULIB_JSONRPC_IMPLEMENTED
#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { MULIB_JSONRPC_MAX_BATCH_ = 1024 };

static mulib_json_t *mulib_jsonrpc_clone_(const mulib_json_t *v) {
  char *s = mulib_json_stringify(v, 0);
  mulib_json_t *copy;
  if (!s) return NULL;
  copy = mulib_json_parse(s, strlen(s), NULL);
  free(s);
  return copy;
}

/* Takes ownership of v even when insertion fails. */
static int mulib_jsonrpc_set_(mulib_json_t *obj, const char *key, mulib_json_t *v) {
  if (v && mulib_json_set(obj, key, v)) return 1;
  mulib_json_destroy(v);
  return 0;
}

static int mulib_jsonrpc_version_(const mulib_json_t *v) {
  size_t n = 0;
  const char *s = mulib_json_text(v, &n);
  return s && n == 3 && memcmp(s, "2.0", 3) == 0;
}

static int mulib_jsonrpc_id_valid_(const mulib_json_t *v) {
  mulib_json_type t = mulib_json_typeof(v);
  return v && (t == MULIB_JSON_NULL || t == MULIB_JSON_STRING || t == MULIB_JSON_NUMBER);
}

static int mulib_jsonrpc_params_valid_(const mulib_json_t *v) {
  mulib_json_type t = mulib_json_typeof(v);
  return !v || t == MULIB_JSON_ARRAY || t == MULIB_JSON_OBJECT;
}

static int mulib_jsonrpc_method_(const char *s, size_t n) {
  return s && n && (n < 4 || memcmp(s, "rpc.", 4) != 0);
}

static int mulib_jsonrpc_request_valid_(const mulib_json_t *v, const mulib_json_t **id_out) {
  const mulib_json_t *id;
  const char *method;
  size_t n = 0;
  if (mulib_json_typeof(v) != MULIB_JSON_OBJECT ||
      !mulib_jsonrpc_version_(mulib_json_get(v, "jsonrpc"))) return 0;
  method = mulib_json_text(mulib_json_get(v, "method"), &n);
  id = mulib_json_get(v, "id");
  if (!mulib_jsonrpc_method_(method, n) || (id && !mulib_jsonrpc_id_valid_(id)) ||
      !mulib_jsonrpc_params_valid_(mulib_json_get(v, "params"))) return 0;
  *id_out = id;
  return 1;
}

/* Return the borrowed ID of a well-formed response, or NULL on failure. */
static const mulib_json_t *mulib_jsonrpc_reply_id_(const mulib_json_t *v, int *remote_error) {
  const mulib_json_t *result, *error, *id;
  if (mulib_json_typeof(v) != MULIB_JSON_OBJECT ||
      !mulib_jsonrpc_version_(mulib_json_get(v, "jsonrpc"))) return NULL;
  id = mulib_json_get(v, "id");
  if (!mulib_jsonrpc_id_valid_(id)) return NULL;
  result = mulib_json_get(v, "result");
  error = mulib_json_get(v, "error");
  if ((result != NULL) == (error != NULL)) return NULL;
  if (error) {
    const mulib_json_t *code;
    double integer;
    if (mulib_json_typeof(error) != MULIB_JSON_OBJECT) return NULL;
    code = mulib_json_get(error, "code");
    if (mulib_json_typeof(code) != MULIB_JSON_NUMBER ||
        modf(mulib_json_numeric(code), &integer) != 0.0 ||
        mulib_json_typeof(mulib_json_get(error, "message")) != MULIB_JSON_STRING)
      return NULL;
    *remote_error = 1;
  }
  return id;
}

/* Inputs are already validated. This total order treats -0 and +0 equally. */
static int mulib_jsonrpc_id_compare_(const mulib_json_t *a, const mulib_json_t *b) {
  mulib_json_type t = mulib_json_typeof(a), u = mulib_json_typeof(b);
  if (t != u) return t < u ? -1 : 1;
  if (t == MULIB_JSON_NUMBER) {
    double x = mulib_json_numeric(a), y = mulib_json_numeric(b);
    return (x > y) - (x < y);
  }
  if (t == MULIB_JSON_STRING) {
    size_t x, y;
    const char *s = mulib_json_text(a, &x), *p = mulib_json_text(b, &y);
    if (x != y) return x < y ? -1 : 1;
    return x ? memcmp(s, p, x) : 0;
  }
  return 0;
}

typedef struct mulib_jsonrpc_id_slot_ {
  const mulib_json_t *id;
  int seen;
} mulib_jsonrpc_id_slot_;

static int mulib_jsonrpc_slot_compare_(const void *a, const void *b) {
  return mulib_jsonrpc_id_compare_(((const mulib_jsonrpc_id_slot_ *)a)->id,
                                  ((const mulib_jsonrpc_id_slot_ *)b)->id);
}

void mulib_cdd_jsonrpc_reply_free(mulib_cdd_jsonrpc_reply_t *r) {
  if (r) {
    mulib_cdd_rest_response_free(&r->response);
    r->remote_error = 0;
  }
}

mulib_json_t *mulib_cdd_jsonrpc_make(const char *method, const mulib_json_t *params,
    const mulib_json_t *id) {
  mulib_json_t *obj;
  if (!method || !mulib_jsonrpc_method_(method, strlen(method)) ||
      (id && !mulib_jsonrpc_id_valid_(id)) || !mulib_jsonrpc_params_valid_(params))
    return NULL;
  obj = mulib_json_new(MULIB_JSON_OBJECT);
  if (!obj) return NULL;
  if (!mulib_jsonrpc_set_(obj, "jsonrpc", mulib_json_string("2.0")) ||
      !mulib_jsonrpc_set_(obj, "method", mulib_json_string(method)) ||
      (id && !mulib_jsonrpc_set_(obj, "id", mulib_jsonrpc_clone_(id))) ||
      (params && !mulib_jsonrpc_set_(obj, "params", mulib_jsonrpc_clone_(params)))) {
    mulib_json_destroy(obj);
    return NULL;
  }
  return obj;
}

int mulib_cdd_jsonrpc_exchange(const mulib_muw3_client_t *client, const char *url,
    const mulib_json_t *request, const mulib_http_header_t *headers, size_t n,
    mulib_cdd_jsonrpc_reply_t *out) {
  mulib_cdd_jsonrpc_reply_t tmp = {0};
  const mulib_json_t *v, *id, *single_id = NULL;
  mulib_jsonrpc_id_slot_ *ids = NULL;
  size_t expected = 0;
  int batch = mulib_json_typeof(request) == MULIB_JSON_ARRAY, rc = -1;
  if (!out || !url || !*url || (!headers && n) || n > 254) return -1;
  if (batch) {
    size_t count = mulib_json_size(request);
    if (!count) return -1;
    if (count > MULIB_JSONRPC_MAX_BATCH_) return MULIB_MUW3_LIMIT;
    for (size_t i = 0; i < count; ++i) {
      if (!mulib_jsonrpc_request_valid_(mulib_json_at(request, i), &id))
        goto done;
      if (!id) continue;
      if (!ids) {
        ids = (mulib_jsonrpc_id_slot_ *)malloc(count * sizeof(*ids));
        if (!ids) return MULIB_MUW3_NOMEM;
      }
      ids[expected++] = (mulib_jsonrpc_id_slot_){id, 0};
    }
    if (expected > 1) {
      qsort(ids, expected, sizeof(*ids), mulib_jsonrpc_slot_compare_);
      for (size_t i = 1; i < expected; ++i)
        if (!mulib_jsonrpc_slot_compare_(&ids[i - 1], &ids[i])) goto done;
    }
  } else {
    if (!mulib_jsonrpc_request_valid_(request, &single_id)) return -1;
    expected = single_id != NULL;
  }

  rc = mulib_cdd_rest_request(client, "POST", url, headers, n, request, 1, &tmp.response);
  if (rc != 1) goto done;
  v = tmp.response.json;
  if (!expected) {
    if (v) goto malformed;
  } else if (batch) {
    if (mulib_json_typeof(v) != MULIB_JSON_ARRAY || mulib_json_size(v) != expected)
      goto malformed;
    for (size_t i = 0; i < expected; ++i) {
      mulib_jsonrpc_id_slot_ key = {
        mulib_jsonrpc_reply_id_(mulib_json_at(v, i), &tmp.remote_error), 0
      };
      mulib_jsonrpc_id_slot_ *match;
      if (!key.id) goto malformed;
      match = (mulib_jsonrpc_id_slot_ *)bsearch(&key, ids, expected, sizeof(*ids),
                                               mulib_jsonrpc_slot_compare_);
      if (!match || match->seen) goto malformed;
      match->seen = 1;
    }
  } else {
    id = mulib_jsonrpc_reply_id_(v, &tmp.remote_error);
    if (!id || mulib_jsonrpc_id_compare_(single_id, id)) goto malformed;
  }
  *out = tmp;
  goto done;
malformed:
  mulib_cdd_jsonrpc_reply_free(&tmp);
  rc = -1;
done:
  free(ids);
  return rc;
}

int mulib_cdd_jsonrpc_call(const mulib_muw3_client_t *client, const char *url,
    const char *method, const mulib_json_t *params, const mulib_json_t *id,
    mulib_cdd_jsonrpc_reply_t *out) {
  mulib_json_t *q;
  int rc;
  if (!out || !url || !*url) return -1;
  q = mulib_cdd_jsonrpc_make(method, params, id);
  if (!q) return -1;
  rc = mulib_cdd_jsonrpc_exchange(client, url, q, NULL, 0, out);
  mulib_json_destroy(q);
  return rc;
}

mulib_json_t *mulib_jsonrpc_make(const char *method, const mulib_json_t *params,
    const mulib_json_t *id) {
  return mulib_cdd_jsonrpc_make(method, params, id);
}

int mulib_jsonrpc_exchange(const mulib_muw3_client_t *client, const char *url,
    const mulib_json_t *request, const mulib_http_header_t *headers, size_t n,
    mulib_jsonrpc_reply_t *out) {
  return mulib_cdd_jsonrpc_exchange(client, url, request, headers, n, out);
}

int mulib_jsonrpc_call(const mulib_muw3_client_t *client, const char *url,
    const char *method, const mulib_json_t *params, const mulib_json_t *id,
    mulib_jsonrpc_reply_t *out) {
  return mulib_cdd_jsonrpc_call(client, url, method, params, id, out);
}

void mulib_jsonrpc_reply_free(mulib_jsonrpc_reply_t *r) {
  mulib_cdd_jsonrpc_reply_free(r);
}
#endif
#endif
