/*
 * On1x SDK compiler/frontend embedding API.
 *
 * This header is C11-compatible and may be included without the SDK umbrella.
 * Objects are opaque and all returned character/byte ranges are borrowed from
 * the object named by the operation. No caller frees SDK-owned memory.
 */
#ifndef ON1X_SDK_COMPILER_H
#define ON1X_SDK_COMPILER_H

#include "On1xSDK-Diagnostic.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_COMPILER_ABI_VERSION 2u
#define ON1X_SDK_FRONTEND_SCHEMA_VERSION 1u

/* Initialize an extensible output record before passing it to the SDK. */
#ifdef __cplusplus
#define ON1X_SDK_COMPILER_INIT(type) \
    []() { \
        type value{}; \
        value.abi_version = ON1X_SDK_COMPILER_ABI_VERSION; \
        value.struct_size = (uint32_t)sizeof(type); \
        return value; \
    }()
#else
#define ON1X_SDK_COMPILER_INIT(type) \
    { .abi_version = ON1X_SDK_COMPILER_ABI_VERSION, \
      .struct_size = (uint32_t)sizeof(type) }
#endif

typedef struct On1xSDK_Session On1xSDK_Session;
typedef struct On1xSDK_Options On1xSDK_Options;
typedef struct On1xSDK_Source On1xSDK_Source;
typedef struct On1xSDK_SourceSnapshot On1xSDK_SourceSnapshot;
typedef struct On1xSDK_VFS On1xSDK_VFS;
typedef struct On1xSDK_TokenStream On1xSDK_TokenStream;
typedef struct On1xSDK_AST On1xSDK_AST;
typedef struct On1xSDK_ASTBuilder On1xSDK_ASTBuilder;
typedef struct On1xSDK_Compilation On1xSDK_Compilation;
typedef struct On1xSDK_Cache On1xSDK_Cache;
typedef struct On1xSDK_Workspace On1xSDK_Workspace;

/*
 * Operations return the shared ABI-2 status directly. CompilerCondition is
 * retained as supplemental frontend detail for diagnostics and compatibility;
 * it never replaces the specific primary result.
 */
typedef uint32_t On1xSDK_CompilerCondition;
enum {
    ON1X_SDK_COMPILER_CONDITION_NONE = 0,
    ON1X_SDK_COMPILER_CANCELLED = 1,
    ON1X_SDK_COMPILER_DEADLINE_EXPIRED = 2,
    ON1X_SDK_COMPILER_STALE_REVISION = 3,
    ON1X_SDK_COMPILER_NOT_FOUND = 4,
    ON1X_SDK_COMPILER_INVALID_STATE = 5,
    ON1X_SDK_COMPILER_VERIFICATION_FAILED = 6,
    ON1X_SDK_COMPILER_CAPABILITY_DENIED = 7,
    ON1X_SDK_COMPILER_IO_FAILURE = 8,
    ON1X_SDK_COMPILER_SCHEMA_MISMATCH = 9,
    ON1X_SDK_COMPILER_CROSS_SESSION = 10
};

typedef uint64_t On1xSDK_CompilerCapability;
enum {
    ON1X_SDK_COMPILER_CAP_SESSIONS = UINT64_C(1) << 0,
    ON1X_SDK_COMPILER_CAP_VFS = UINT64_C(1) << 1,
    ON1X_SDK_COMPILER_CAP_CANONICAL_LEXER = UINT64_C(1) << 2,
    ON1X_SDK_COMPILER_CAP_CANONICAL_PARSER = UINT64_C(1) << 3,
    ON1X_SDK_COMPILER_CAP_AST_VIEW = UINT64_C(1) << 4,
    ON1X_SDK_COMPILER_CAP_SOURCE_CANDIDATE = UINT64_C(1) << 5,
    ON1X_SDK_COMPILER_CAP_PPX_V1_READMISSION = UINT64_C(1) << 6,
    ON1X_SDK_COMPILER_CAP_MACRO_EXPANSION = UINT64_C(1) << 7,
    ON1X_SDK_COMPILER_CAP_MODULE_LOADING = UINT64_C(1) << 8,
    ON1X_SDK_COMPILER_CAP_INCREMENTAL_REVISIONS = UINT64_C(1) << 9
};

typedef struct On1xSDK_CompilerInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t capabilities;
    uint32_t frontend_schema_version;
    uint32_t ppx_schema_version;
    const char *scanner_profile;
    size_t scanner_profile_size;
    const char *ppx_schema_format;
    size_t ppx_schema_format_size;
} On1xSDK_CompilerInfo;

/*
 * Writes only fields covered by the caller-provided struct_size. The caller
 * must initialize the ABI prefix with ON1X_SDK_COMPILER_INIT.
 */
On1xSDK_Status on1x_sdk_compiler_info(On1xSDK_CompilerInfo *out_info);

/* ABI-v2 allocator integration is intentionally not duplicated here. */
On1xSDK_Status on1x_sdk_compiler_allocator_supported(uint32_t *out_supported);

#ifdef __cplusplus
}
#endif

#endif
