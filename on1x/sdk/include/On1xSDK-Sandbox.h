#ifndef ON1X_SDK_SANDBOX_H
#define ON1X_SDK_SANDBOX_H

#include "On1xSDK-PluginManifest.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_SandboxEnforcement;
enum {
    ON1X_SDK_SANDBOX_SECCOMP = 1u << 0,
    ON1X_SDK_SANDBOX_RLIMIT = 1u << 1,
    ON1X_SDK_SANDBOX_CREDENTIALS = 1u << 2,
    ON1X_SDK_SANDBOX_NAMESPACE = 1u << 3,
    ON1X_SDK_SANDBOX_CGROUP = 1u << 4,
    ON1X_SDK_SANDBOX_FILESYSTEM = 1u << 5,
    ON1X_SDK_SANDBOX_NETWORK = 1u << 6
};

typedef struct On1xSDK_SandboxProfile {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t required_enforcement;
    uint32_t reserved;
    uint64_t address_space_bytes;
    uint64_t stack_bytes;
    uint64_t cpu_seconds;
    uint64_t wall_deadline_ms;
    uint64_t startup_deadline_ms;
    uint32_t max_processes;
    uint32_t max_open_files;
    uint32_t max_control_frame_bytes;
    uint32_t max_broker_requests;
} On1xSDK_SandboxProfile;

typedef struct On1xSDK_SandboxSupport {
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t enforceable;
    uint32_t missing;
    On1xSDK_Status status;
} On1xSDK_SandboxSupport;

On1xSDK_SandboxProfile on1x_sdk_sandbox_profile_default(void);
On1xSDK_Status on1x_sdk_sandbox_profile_validate(
    const On1xSDK_SandboxProfile *profile
);
/*
 * This is a capability query, not an isolation claim. Actual worker setup may
 * still fail and must then terminate activation without in-process fallback.
 */
On1xSDK_Status on1x_sdk_sandbox_support(
    const On1xSDK_SandboxProfile *profile,
    On1xSDK_SandboxSupport *out
);

#ifdef __cplusplus
}
#endif

#endif
