#ifndef ON1X_STDLIB_HTTP_RUNTIME_H
#define ON1X_STDLIB_HTTP_RUNTIME_H
#include <stdint.h>
#include "mulib-x509.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct on1x_http_grant_v1 {
  /* A nonzero per-socket wait, no greater than 30000 ms. */
  unsigned wait_ms;
  /* Required for HTTPS. Host-owned immutable store, valid until exchange
   * returns. No implicit file, environment, or system trust lookup. */
  const mulib_x509_store_t *trust;
} on1x_http_grant_v1;

/* REQUIRED host link symbol. No weak or default allow implementation.
 * Called once, before transport/trust operations, with validated URL, exact
 * canonical numeric address, effective port, TLS boolean and GET/HEAD boolean.
 * Strings and grant are borrowed for this call. Return exactly 1 and fill the
 * grant to approve; otherwise fail closed. The host owns egress and budget
 * policy and should charge attempts here, even when the transport later fails.
 * Must not reenter the bridge or retain its arguments. */
int on1x_http_authorize_v1(const char *url, const char *address, unsigned port,
                           int tls, int head, on1x_http_grant_v1 *grant);

/* Raw generated-C ABI. Borrowed NUL-free string, synchronous immutable On1x
 * snapshot result. No views, handles, or callbacks cross the On1x boundary. */
const char *on1x_http_exchange_v1(const char *url, int64_t head);

#ifdef __cplusplus
}
#endif
#endif
