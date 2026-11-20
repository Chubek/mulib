#ifndef ON1X_KERNEL_IPC_H
#define ON1X_KERNEL_IPC_H

#include "on1x_runtime.h"
#if !defined(ON1X_KERNEL_ABI_V1) || ON1X_KERNEL_ABI_V1 != 1 || \
    ON1X_CLOSURE_ABI_VERSION != 1 || ON1X_UNIQUE_ABI_VERSION != 1 || \
    !defined(__linux__) || !defined(__x86_64__) || defined(__ILP32__)
#error "framed KernelBind requires the native Linux x86-64 ABI profile"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Private generated C11 ABI. No pointer, descriptor, or scalar token is
 * exposed to On1x. Both calls publish outputs only on success. */
typedef int64_t (*on1x_kernel_ipc_callback)(on1x_env *, int64_t);
int64_t on1x_kernel_ipc_v1_open_pair(on1x_unique **out);
/* Consumes *owner on every outcome before possibly calling callback. The
 * callback has one synchronous, same-thread invocation at most, after close;
 * neither its code pointer nor its borrowed context is retained. */
int64_t on1x_kernel_ipc_v1_roundtrip(on1x_unique **owner, uint8_t byte,
        int64_t timeout_ms, on1x_kernel_ipc_callback callback,
        on1x_env *context, int64_t *out);

/* These accept a distinct session owner. All operations and the owner's
 * destruction belong to the opening thread. Session calls borrow the box
 * exclusively for the call (including argument evaluation in generated code).
 * Do not consume the box via the generic C ABI from a callback. */
int64_t on1x_kernel_ipc_v1_open_session(on1x_unique **out);
int64_t on1x_kernel_ipc_v1_register_callback(on1x_unique *owner,
        on1x_kernel_ipc_callback callback, on1x_env *context);
int64_t on1x_kernel_ipc_v1_revoke(on1x_unique *owner);
int64_t on1x_kernel_ipc_v1_send(on1x_unique *owner, uint8_t byte, int64_t timeout_ms);
int64_t on1x_kernel_ipc_v1_poll(on1x_unique *owner, int64_t timeout_ms, int64_t *out);
int64_t on1x_kernel_ipc_v1_finish_send(on1x_unique *owner);

#ifdef __cplusplus
}
#endif
#endif
