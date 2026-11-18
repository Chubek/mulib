#ifndef ON1X_PRETTYPRINT_RUNTIME_H
#define ON1X_PRETTYPRINT_RUNTIME_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Raw C ABI; neither function is thread-safe. expr is a borrowed,
 * NUL-terminated S-expression, or NULL. The nonnull result is immutable and
 * must not be freed. Successful snapshots remain valid until process-exit
 * cleanup; a failed call returns a static empty string.
 * paragraph: 0 word greedy, 1 minimum raggedness, 2 Knuth-Plass. */
const char *on1x_pretty_render(const char *expr,int64_t width,int64_t algorithm,
                               int64_t target,int64_t paragraph);
/* Read immediately after render: 0 success, -1 invalid arguments,
 * -2 kernel failure (parse/layout/emission/allocation), -3 bridge allocation
 * or cleanup-registration failure, -4 input/emitted-output/pool limit. */
int64_t on1x_pretty_status(void);
#ifdef __cplusplus
}
#endif
#endif
