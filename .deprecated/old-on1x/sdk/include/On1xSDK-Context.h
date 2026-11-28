#ifndef ON1X_SDK_CONTEXT_H
#define ON1X_SDK_CONTEXT_H

#include "On1xSDK-Allocator.h"
#include "On1xSDK-Diagnostic.h"
#include "On1xSDK-Handle.h"
#include "On1xSDK-String.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_CANDIDATE_BYTES (64u * 1024u * 1024u)

typedef uint32_t On1xSDK_LogLevel;
enum {
    ON1X_SDK_LOG_DEBUG = 1,
    ON1X_SDK_LOG_INFO = 2,
    ON1X_SDK_LOG_WARNING = 3,
    ON1X_SDK_LOG_ERROR = 4
};

typedef On1xSDK_Status (*On1xSDK_LogFn)(
    void *userdata,
    On1xSDK_LogLevel level,
    On1xSDK_StringView target,
    On1xSDK_StringView message
);

typedef On1xSDK_Status (*On1xSDK_SubmitCandidateFn)(
    void *userdata,
    On1xSDK_StringView schema,
    uint32_t schema_version,
    On1xSDK_ByteView candidate,
    uint64_t source_revision,
    On1xSDK_Handle *candidate_handle
);

typedef struct On1xSDK_ContextConfig {
    uint32_t abi_version;
    uint32_t struct_size;
    const On1xSDK_Allocator *allocator;
    On1xSDK_DiagnosticSinkV2 diagnostics;
    On1xSDK_LogFn log;
    void *log_userdata;
    On1xSDK_SubmitCandidateFn submit_candidate;
    void *submit_userdata;
    uint32_t max_handles;
    uint32_t reserved;
} On1xSDK_ContextConfig;

On1xSDK_Status on1x_sdk_context_create(
    const On1xSDK_ContextConfig *config, On1xSDK_Context **out
);
void on1x_sdk_context_retain(On1xSDK_Context *context);
void on1x_sdk_context_release(On1xSDK_Context *context);
uint64_t on1x_sdk_context_owner(const On1xSDK_Context *context);
const On1xSDK_Allocator *on1x_sdk_context_allocator(
    const On1xSDK_Context *context
);
const On1xSDK_Host *on1x_sdk_context_host(const On1xSDK_Context *context);

#ifdef __cplusplus
}
#endif

#endif
