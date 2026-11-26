#ifndef ON1X_STDLIB_ELVI_LIVE_H
#define ON1X_STDLIB_ELVI_LIVE_H

#include "capture.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { ON1X_ELVI_LIVE_MAX_BODY = 65536 };

typedef struct {
  const char *url;
  const char *host;
  size_t max_body_bytes;
} on1x_elvi_live_request_v1;

typedef struct {
  const char *approved_host;
  const mulib_elvi_scrape_https_t *https;
  const char *approved_home;
  const char *approved_cookie_path;
} on1x_elvi_live_grant_v1;

/* Required host symbol. No default provider. The request is borrowed for this
 * invocation only. Grant storage and callbacks must remain valid until fetch
 * returns. Return exactly 1 to approve a checked URL; independently authorize
 * the destination (including at connect), cookie file, TLS trust and work
 * budget. Implement robots, rate, and wall-clock policies in the trusted host
 * or its supervisor. Do not reenter the bridge or retain request pointers. */
int on1x_elvi_live_authorize_v1(const on1x_elvi_live_request_v1 *request,
                               on1x_elvi_live_grant_v1 *grant);

/* Raw generated-C ABI. Both inputs borrow NUL-free On1x text for this call.
 * Empty url selects the compiled home. The return is an immutable On1x text
 * snapshot (or a static error); no foreign allocation escapes. */
const char *on1x_elvi_live_snapshot_v1(const char *source, const char *url,
                                       int64_t limit);

#ifdef __cplusplus
}
#endif
#endif
