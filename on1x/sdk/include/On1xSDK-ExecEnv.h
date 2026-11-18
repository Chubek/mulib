/* Inert execution-environment requests; not a launcher or sandbox. */
#ifndef ON1X_SDK_EXECENV_H
#define ON1X_SDK_EXECENV_H

#include "On1xSDK-Types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_EXECENV_DESCRIPTION_BYTES 1024u

enum {
    ON1X_SDK_EXECENV_FS_READ = 1u << 0,
    ON1X_SDK_EXECENV_FS_WRITE = 1u << 1,
    ON1X_SDK_EXECENV_NETWORK = 1u << 2,
    ON1X_SDK_EXECENV_SUBPROCESS = 1u << 3
};

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    const char *toolchain;           /* Optional logical toolchain name; not resolved. */
    uint32_t requested_capabilities; /* Bitmask of the above (0 requests none). */
    uint32_t requested_timeout_ms;   /* 0 means unspecified; never enforced here. */
    const char *description;         /* Optional nonempty printable ASCII. */
} On1xSDK_ExecEnv;

/*
 * Copies requests, never grants capabilities or establishes restrictions.
 * No sandboxing, process creation, shell execution, binary resolution, loading
 * or toolchain selection is performed. A toolchain name need not be registered.
 */
On1xSDK_Status on1x_sdk_execenv_register(On1xSDK_Registry *registry,
                                         const On1xSDK_ExecEnv *environment);
const On1xSDK_ExecEnv *on1x_sdk_execenv_find(const On1xSDK_Registry *registry,
                                           const char *name);
size_t on1x_sdk_execenv_count(const On1xSDK_Registry *registry);
const On1xSDK_ExecEnv *on1x_sdk_execenv_at(const On1xSDK_Registry *registry, size_t index);

#ifdef __cplusplus
}
#endif

#endif
