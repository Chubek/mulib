#ifndef ON1X_GENERATED_SCAN_H
#define ON1X_GENERATED_SCAN_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*on1x_token_callback)(void *context, const char *text, size_t offset);
/* Internal scanner interface. 0 success, 1 syntax error, 2 allocation error.
 * Input must point to length bytes without NUL; no terminator is required.
 * scan() enforces source limits and NUL rejection. Calls have independent state.
 * Callback text is temporary; callers must copy it before returning. */
int on1x_generated_scan(const char *input, size_t length, on1x_token_callback callback,
                        void *context, size_t *error_offset);
/* Generated scanner only. Apply one fault at a canonical lexical error offset.
 * Input is NUL-terminated and shorter than 1 MiB. 1 returns malloc-owned
 * proposal text via replacement, 0 means no patch, -1 is allocation failure. */
int on1x_aurocks_fault_step(const char *input, size_t offset, char **replacement);
#ifdef __cplusplus
}
#endif
#endif
