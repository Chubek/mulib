/** @file mulib-opencode-client.h
 * @brief Schema-derived OpenCode V2 HTTP client and read-only Mune binding.
 * Define MULIB_IMPL_CDD_MUNE_OPENCODE_CLIENT in one implementation TU. */
#ifndef MULIB_MUNE_OPENCODE_CLIENT_H
#define MULIB_MUNE_OPENCODE_CLIENT_H
#include "mulib-muw3.h"
#include "mulib-mune.h"
#include "mulib-json.h"
#include "mulib-sse.h"
#include "mulib-url.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum mulib_cdd_opencode_status {
  MULIB_CDD_OPENCODE_OK = 1, MULIB_CDD_OPENCODE_INVALID = -1,
  MULIB_CDD_OPENCODE_LIMIT = -2, MULIB_CDD_OPENCODE_NOMEM = -3,
  MULIB_CDD_OPENCODE_TRANSPORT = -4, MULIB_CDD_OPENCODE_CANCELLED = -7,
  MULIB_CDD_OPENCODE_HTTP_ERROR = -8, MULIB_CDD_OPENCODE_PROTOCOL = -9,
  MULIB_CDD_OPENCODE_UNSUPPORTED = -10, MULIB_CDD_OPENCODE_STREAM_ERROR = -11
} mulib_cdd_opencode_status_t;

typedef struct mulib_cdd_opencode_operation {
  const char *id, *method, *path;
  const char *query_names, *required_fields; /* comma-separated; query '*' repeated, '!' required */
  unsigned success_status, flags; /* bit 0: JSON request; bit 1: SSE response */
} mulib_cdd_opencode_operation_t;

typedef struct mulib_cdd_opencode_param {
  const char *name, *value;
} mulib_cdd_opencode_param_t;

typedef struct mulib_cdd_opencode_client {
  mulib_muw3_client_t transport;
  const char *origin; /* borrowed origin, no path except optional "/" */
  const char *authorization; /* borrowed complete Authorization value, optional */
  size_t max_event_bytes; /* zero selects 1 MiB */
  int allow_http; /* explicit cleartext opt-in; default is HTTPS only */
  int allow_session_writes; /* opt in to single C API session mutations */
} mulib_cdd_opencode_client_t;

typedef int (*mulib_cdd_opencode_event_fn)(void *, const mulib_sse_event_t *,
                                           const mulib_json_t *);
typedef struct mulib_cdd_opencode_request {
  const char *operation_id;
  const mulib_cdd_opencode_param_t *path, *query;
  size_t path_count, query_count;
  const mulib_json_t *json;
  mulib_cdd_opencode_event_fn event; /* required on SSE operations; 1 continues */
  void *event_user;
} mulib_cdd_opencode_request_t;

typedef struct mulib_cdd_opencode_response {
  mulib_muw3_response_t http;
  mulib_json_t *json; /* owned; NULL for no-content or SSE responses */
} mulib_cdd_opencode_response_t;

size_t mulib_cdd_opencode_operation_count(void);
const mulib_cdd_opencode_operation_t *mulib_cdd_opencode_operation_at(size_t index);
const mulib_cdd_opencode_operation_t *mulib_cdd_opencode_operation(const char *id);
/* Use a trusted, explicit endpoint. Does not discover or start a service. */
mulib_cdd_opencode_client_t mulib_cdd_opencode_client(const char *origin);
/* On OK or HTTP_ERROR, transfer an owned response into an empty destination.
 * Other errors leave it unchanged. SSE callback arguments are borrowed;
 * delivered events cannot be undone if the stream subsequently fails. */
mulib_cdd_opencode_status_t mulib_cdd_opencode_request(
    const mulib_cdd_opencode_client_t *, const mulib_cdd_opencode_request_t *,
    mulib_cdd_opencode_response_t *);
void mulib_cdd_opencode_response_free(mulib_cdd_opencode_response_t *);
/* Install/replace opencode_request(id[, path[, query]]) in an idle, trusted Mune
 * state. Only buffered GET operations are available to scripts. Client and
 * borrowed configuration must outlive the state and any copied closure. */
mulib_mune_status_t mulib_cdd_opencode_bind(mulib_mune_state_t *,
                                           const mulib_cdd_opencode_client_t *);

#ifdef __cplusplus
}
#endif
#endif

#if defined(MULIB_IMPL_CDD_MUNE_OPENCODE_CLIENT)
#include "mulib-buffer.h"
#include "mulib-uri.h"
#include "mulib-utf8.h"
#include "mune-opencode-client.h"
#endif
