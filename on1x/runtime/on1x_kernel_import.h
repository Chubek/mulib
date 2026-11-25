#ifndef ON1X_KERNEL_IMPORT_H
#define ON1X_KERNEL_IMPORT_H

#include <stddef.h>
#include <stdint.h>

#define ON1X_KERNEL_IMPORT_TABLE_ABI_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

/* Private C11 code-generation ABI for load-time KernelBind. The requirement
 * index is the dispatch slot; each digest is 64 lowercase hex characters.
 * The version field is the required module version, not the table ABI. */
struct on1x_kernel_import {
    const char *module;
    const char *export_name;
    unsigned version;
    unsigned arity;
    const char *interface_sha256;
    const char *behavior_sha256;
};

/* Initialize before running application code. A nonzero result is diagnosed
 * on stderr and transfers no ownership from the caller. Failures never
 * publish a partial table and can be retried. The first complete table is
 * immutable and lives for the process lifetime; later calls must match it.
 * Zero requirements are a no-op if no table has been published.
 * Do not mutate the process environment during concurrent initialization. */
int on1x_kernel_import_init(const struct on1x_kernel_import *required, size_t count);

/* A published identity is borrowed for the process lifetime. Returns NULL
 * if there is no complete table or if the slot does not exist. */
const struct on1x_kernel_import *on1x_kernel_import_identity(size_t slot);

/* Dispatch is only valid after initialization. A malformed dispatch or
 * checked-kernel failure is diagnosed and terminates with status 70. */
int64_t on1x_kernel_call2(size_t slot, int64_t a, int64_t b);
int64_t on1x_kernel_call3(size_t slot, int64_t a, int64_t b, int64_t c);

#ifdef __cplusplus
}
#endif
#endif
