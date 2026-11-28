#ifndef ON1X_LUAEMBED_RUNTIME_H
#define ON1X_LUAEMBED_RUNTIME_H
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Trusted build-host, synchronous, non-thread-safe raw scalar ABI.
 * Inputs are borrowed for the call. Output/error views last until the next
 * eval; clients must copy them before evaluating another carrier. No pointers
 * to Mune objects escape. Failure never publishes partial captured output. */
bool on1x_luaembed_eval(const char *source, const char *location,
                      const char *label, const char *input);
const char *on1x_luaembed_output(void);
const char *on1x_luaembed_error(void);
int64_t on1x_luaembed_offset(void);
const char *on1x_luaembed_message(const char *key);
#ifdef __cplusplus
}
#endif
#endif
