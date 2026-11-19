/* Named toolchain identities, not executable paths or command lines. */
#ifndef ON1X_SDK_TOOLCHAIN_H
#define ON1X_SDK_TOOLCHAIN_H

#include "On1xSDK-Target.h"
#include "On1xSDK-Types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    const char *target;    /* Syntactic target label, e.g. "x86_64-unknown-linux-gnu". */
    const char *driver_id; /* Logical identifier using registration-name syntax. */
    const char *version;   /* Nonempty printable ASCII, at most ON1X_SDK_NAME_MAX bytes. */
} On1xSDK_Toolchain;

/*
 * Copies metadata only. Target labels have at least two components separated
 * by '-', each matching [A-Za-z0-9][A-Za-z0-9_.]*, at most ON1X_SDK_NAME_MAX
 * bytes total. Registration does not establish target support, look up a
 * driver, resolve a binary, shell out, execute code, or change compiler setup.
 */
On1xSDK_Status on1x_sdk_toolchain_register(On1xSDK_Registry *registry,
                                          const On1xSDK_Toolchain *toolchain);
const On1xSDK_Toolchain *on1x_sdk_toolchain_find(const On1xSDK_Registry *registry,
                                                const char *name);
size_t on1x_sdk_toolchain_count(const On1xSDK_Registry *registry);
const On1xSDK_Toolchain *on1x_sdk_toolchain_at(const On1xSDK_Registry *registry, size_t index);

/* SDK 2 declarative toolchain providers. Registration grants no execution. */
#define ON1X_SDK_TOOLCHAIN_PROVIDER_ABI_VERSION 2u
#define ON1X_SDK_TOOL_DIGEST_BYTES 32u

typedef uint32_t On1xSDK_ToolRole;
enum {
    ON1X_SDK_TOOL_C_COMPILER = 1u << 0,
    ON1X_SDK_TOOL_ASSEMBLER = 1u << 1,
    ON1X_SDK_TOOL_LINKER = 1u << 2,
    ON1X_SDK_TOOL_ARCHIVER = 1u << 3,
    ON1X_SDK_TOOL_OBJDUMP = 1u << 4
};

typedef struct On1xSDK_ToolchainProvider {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    size_t name_size;
    const On1xSDK_Target *target; /* Borrowed for registration; copied by value. */
    const char *driver_id;
    size_t driver_id_size;
    const char *version;
    size_t version_size;
    int32_t priority;
    uint32_t roles;
    uint8_t driver_sha256[ON1X_SDK_TOOL_DIGEST_BYTES];
} On1xSDK_ToolchainProvider;

/*
 * Deep-copies metadata into the shared registry. A provider identity and digest
 * are claims only; a host resolver must separately authorize an exact binary.
 */
On1xSDK_Status on1x_sdk_toolchain_provider_register(
    On1xSDK_Registry *registry, const On1xSDK_ToolchainProvider *provider);

#ifdef __cplusplus
}
#endif

#endif
