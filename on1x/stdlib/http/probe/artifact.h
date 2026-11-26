#ifndef ON1X_STDLIB_HTTP_PROBE_ARTIFACT_H
#define ON1X_STDLIB_HTTP_PROBE_ARTIFACT_H

#include "mulib-workspace-fs.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { ON1X_HTTP_PROBE_ARTIFACT_MAX_BYTES = 256 * 1024 };

typedef struct {
  const mulib_wfs_t *workspace; /* Host-owned, private and local. */
  const char *relative_path;    /* Exact destination independently approved. */
} on1x_http_probe_artifact_grant_v1;

/* Strong host link symbol. `slot` is a symbolic key, never a file path.
 * Approve a destination and charge the budget for this attempt. A grant is
 * borrowed until save returns; do not retain inputs or reenter the bridge. */
int on1x_http_probe_artifact_authorize_v1(const char *slot, size_t length,
                                          on1x_http_probe_artifact_grant_v1 *grant);

/* Generated-C ABI: borrowed strings, no retained pointers. 0 committed,
 * 1 visible but durability uncertain, -1 invalid, -2 denied, -3 limit,
 * -4 destination exists, -5 I/O failure. Never overwrites an existing file. */
int64_t on1x_http_probe_artifact_write_v1(const char *slot, const char *html);

#ifdef __cplusplus
}
#endif
#endif
