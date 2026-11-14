#ifndef ON1X_GENERATED_SCAN_H
#define ON1X_GENERATED_SCAN_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*on1x_token_callback)(void *context, const char *text, size_t offset);
/* Internal scanner interface. 0 success, 1 syntax error, 2 allocation error.
 * Input must point to length bytes without NUL; no terminator is required.
 * scan() enforces source limits/NUL rejection and serializes generated calls.
 * Callback text is temporary; callers must copy it before returning. */
int on1x_generated_scan(const char *input, size_t length, on1x_token_callback callback,
                        void *context, size_t *error_offset);
#ifdef __cplusplus
}
#endif
#endif
