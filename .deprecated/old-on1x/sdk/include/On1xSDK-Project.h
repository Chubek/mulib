/* On1x SDK 2 project-service common types.
 *
 * This header is independently includable from C11 and C++.  Project handles
 * and every byte reachable from their borrowed views use the caller-supplied
 * allocator.  The allocator callbacks and context must outlive those handles.
 */
#ifndef ON1X_SDK_PROJECT_H
#define ON1X_SDK_PROJECT_H

#include "On1xSDK-Allocator.h"
#include "On1xSDK-Cancellation.h"
#include "On1xSDK-Diagnostic.h"
#include "On1xSDK-Status.h"
#include "On1xSDK-String.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef ON1X_SDK_PROJECT_API
#  if defined(_WIN32)
#    if defined(ON1X_SDK_PROJECT_BUILD)
#      define ON1X_SDK_PROJECT_API __declspec(dllexport)
#    else
#      define ON1X_SDK_PROJECT_API __declspec(dllimport)
#    endif
#  elif defined(__GNUC__) || defined(__clang__)
#    define ON1X_SDK_PROJECT_API __attribute__((visibility("default")))
#  else
#    define ON1X_SDK_PROJECT_API
#  endif
#endif

#define ON1X_SDK_PROJECT_ABI_VERSION ON1X_SDK_ABI_VERSION_V2
#define ON1X_SDK_PROJECT_MAX_PATH_BYTES 1024u
#define ON1X_SDK_PROJECT_SHA256_BYTES 32u
#define ON1X_SDK_PROJECT_SHA256_TEXT_BYTES 64u
#define ON1X_SDK_PROJECT_MAX_RELATED 8u

typedef struct On1xSDK_ProjectBuffer On1xSDK_ProjectBuffer;

/* Compatibility names over the shared ABI-2 ownership and view contracts. */
typedef On1xSDK_StringView On1xSDK_ProjectString;
typedef On1xSDK_ByteView On1xSDK_ProjectBytes;
typedef On1xSDK_AllocateFn On1xSDK_ProjectAllocateFn;
typedef On1xSDK_ReallocateFn On1xSDK_ProjectReallocateFn;
typedef On1xSDK_ReleaseFn On1xSDK_ProjectReleaseFn;
typedef On1xSDK_Allocator On1xSDK_ProjectAllocator;

enum {
    ON1X_SDK_PROJECT_NOTE = ON1X_SDK_NOTE,
    ON1X_SDK_PROJECT_WARNING = ON1X_SDK_WARNING,
    ON1X_SDK_PROJECT_ERROR = ON1X_SDK_DIAGNOSTIC_ERROR
};

typedef On1xSDK_SourceSpan On1xSDK_ProjectSpan;
typedef On1xSDK_DiagnosticV2 On1xSDK_ProjectDiagnostic;
typedef On1xSDK_DiagnosticFnV2 On1xSDK_ProjectDiagnosticFn;
typedef On1xSDK_DiagnosticSinkV2 On1xSDK_ProjectDiagnosticSink;

/*
 * Validate and copy a normalized project-relative path.  Accepted paths use
 * '/' separators, contain at most 32 nonempty components, and contain no '.',
 * '..', control bytes, backslashes, or absolute/root syntax.  A single "."
 * is accepted only when allow_root_dot is nonzero.  out_size receives the
 * required byte count even when output_capacity is too small.  The output is
 * not NUL-terminated.
 */
ON1X_SDK_PROJECT_API On1xSDK_Status on1x_sdk_project_normalize_path(
    On1xSDK_ProjectString input, int allow_root_dot,
    char *output, size_t output_capacity, size_t *out_size);

/* Deterministic SHA-256 over an arbitrary length-aware byte view. */
ON1X_SDK_PROJECT_API On1xSDK_Status on1x_sdk_project_sha256(
    On1xSDK_ProjectBytes input,
    unsigned char output[ON1X_SDK_PROJECT_SHA256_BYTES]);

/* Owned buffers are allocated as one block in the recorded host domain. */
ON1X_SDK_PROJECT_API On1xSDK_ProjectBytes on1x_sdk_project_buffer_view(
    const On1xSDK_ProjectBuffer *buffer);
ON1X_SDK_PROJECT_API void on1x_sdk_project_buffer_destroy(
    On1xSDK_ProjectBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
