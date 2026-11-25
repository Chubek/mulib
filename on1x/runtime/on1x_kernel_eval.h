#ifndef ON1X_KERNEL_EVAL_H
#define ON1X_KERNEL_EVAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct on1x_kernel_verified_export;

enum on1x_kernel_eval_status {
    ON1X_KERNEL_EVAL_OK = 0,
    ON1X_KERNEL_EVAL_GUARD = 1,
    ON1X_KERNEL_EVAL_OVERFLOW = 2,
    ON1X_KERNEL_EVAL_INVALID = 3,
};

/* Private fallback executor; recipe must be an immutable verified export.
 * The caller is responsible for checking the behavior pin before dispatch.
 * Evaluates the guard before the body, and writes *out only on success.
 * Neither recipe nor arguments are retained. */
int on1x_kernel_eval(const struct on1x_kernel_verified_export *recipe,
                     const int64_t *args, size_t arity, int64_t *out);

#ifdef __cplusplus
}
#endif
#endif
