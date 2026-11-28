/* Host-authorized exact toolchain resolution without PATH or shell lookup. */
#ifndef ON1X_SDK_TOOLCHAIN_RESOLVER_H
#define ON1X_SDK_TOOLCHAIN_RESOLVER_H

#include "On1xSDK-Registry.h"
#include "On1xSDK-Toolchain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_TOOLCHAIN_BINDINGS 256u
#define ON1X_SDK_MAX_TOOL_PATH_BYTES 4096u

typedef struct On1xSDK_ToolchainResolver On1xSDK_ToolchainResolver;
typedef struct On1xSDK_ToolResolution On1xSDK_ToolResolution;

typedef struct On1xSDK_ToolBinding {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *driver_id;
    size_t driver_id_size;
    uint32_t roles;
    uint32_t reserved;
    const char *absolute_path;
    size_t absolute_path_size;
    uint8_t sha256[ON1X_SDK_TOOL_DIGEST_BYTES];
} On1xSDK_ToolBinding;

/*
 * A resolver is a host-owned allowlist, not extension metadata. max_bindings is
 * a literal finite bound in 1..ON1X_SDK_MAX_TOOLCHAIN_BINDINGS.
 */
On1xSDK_Status on1x_sdk_toolchain_resolver_create(
    size_t max_bindings, On1xSDK_ToolchainResolver **out_resolver);
void on1x_sdk_toolchain_resolver_destroy(On1xSDK_ToolchainResolver *resolver);

/*
 * Authorizes one exact absolute path for logical driver roles and digest.
 * Paths must be canonical POSIX absolute paths: no empty, '.', or '..'
 * components, control bytes, shell metacharacters, or trailing slash.
 * This operation neither opens nor executes the path.
 */
On1xSDK_Status on1x_sdk_toolchain_resolver_authorize(
    On1xSDK_ToolchainResolver *resolver, const On1xSDK_ToolBinding *binding);

typedef struct On1xSDK_ToolchainRequest {
    uint32_t abi_version;
    uint32_t struct_size;
    const On1xSDK_Target *target;
    On1xSDK_ToolRole role; /* Exactly one role bit. */
    const char *provider_name; /* Optional exact provider name. */
    size_t provider_name_size;
} On1xSDK_ToolchainRequest;

typedef struct On1xSDK_ToolResolutionView {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *provider_name;
    size_t provider_name_size;
    const char *driver_id;
    size_t driver_id_size;
    const char *version;
    size_t version_size;
    const char *target_triple;
    size_t target_triple_size;
    On1xSDK_ToolRole role;
    uint32_t reserved;
    const char *absolute_path;
    size_t absolute_path_size;
    uint8_t sha256[ON1X_SDK_TOOL_DIGEST_BYTES];
} On1xSDK_ToolResolutionView;

/*
 * Selects the unique highest-priority compatible provider, then requires an
 * exact host binding with the same driver identity, role, and digest. It never
 * consults PATH, environment variables, executable names, or a shell.
 */
On1xSDK_Status on1x_sdk_toolchain_resolve(
    const On1xSDK_Registry *registry,
    const On1xSDK_ToolchainResolver *resolver,
    const On1xSDK_ToolchainRequest *request,
    On1xSDK_ToolResolution **out_resolution);
On1xSDK_Status on1x_sdk_tool_resolution_view(
    const On1xSDK_ToolResolution *resolution,
    const On1xSDK_ToolResolutionView **out_view);
void on1x_sdk_tool_resolution_destroy(On1xSDK_ToolResolution *resolution);

#ifdef __cplusplus
}
#endif

#endif
