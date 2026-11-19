/* On1x SDK 2 project-service common types.
 *
 * This header is independently includable from C11 and C++.  Project handles
 * and every byte reachable from their borrowed views use the caller-supplied
 * allocator.  The allocator callbacks and context must outlive those handles.
 */
#ifndef ON1X_SDK_PROJECT_H
#define ON1X_SDK_PROJECT_H

#include "On1xSDK-Types.h"

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

#define ON1X_SDK_PROJECT_ABI_VERSION 2u
#define ON1X_SDK_PROJECT_MAX_PATH_BYTES 1024u
#define ON1X_SDK_PROJECT_SHA256_BYTES 32u
#define ON1X_SDK_PROJECT_SHA256_TEXT_BYTES 64u
#define ON1X_SDK_PROJECT_MAX_RELATED 8u

typedef struct On1xSDK_ProjectBuffer On1xSDK_ProjectBuffer;

typedef struct On1xSDK_ProjectString {
    const char *data; /* May be NULL when size is zero.  Not necessarily NUL-terminated. */
    size_t size;
} On1xSDK_ProjectString;

typedef struct On1xSDK_ProjectBytes {
    const unsigned char *data; /* May be NULL when size is zero. */
    size_t size;
} On1xSDK_ProjectBytes;

typedef void *(*On1xSDK_ProjectAllocateFn)(void *context, size_t size, size_t alignment);
typedef void *(*On1xSDK_ProjectReallocateFn)(void *context, void *pointer,
                                             size_t old_size, size_t new_size,
                                             size_t alignment);
typedef void (*On1xSDK_ProjectReleaseFn)(void *context, void *pointer,
                                         size_t size, size_t alignment);

typedef struct On1xSDK_ProjectAllocator {
    uint32_t abi_version;
    uint32_t struct_size;
    void *context;
    On1xSDK_ProjectAllocateFn allocate;
    On1xSDK_ProjectReallocateFn reallocate; /* Optional; current services do not call it. */
    On1xSDK_ProjectReleaseFn release;
} On1xSDK_ProjectAllocator;

enum {
    ON1X_SDK_PROJECT_NOTE = 0,
    ON1X_SDK_PROJECT_WARNING = 1,
    ON1X_SDK_PROJECT_ERROR = 2
};

typedef struct On1xSDK_ProjectSpan {
    uint32_t abi_version;
    uint32_t struct_size;
    unsigned char source_revision[ON1X_SDK_PROJECT_SHA256_BYTES];
    size_t begin; /* Zero-based half-open byte offsets in the exact input revision. */
    size_t end;
} On1xSDK_ProjectSpan;

typedef struct On1xSDK_ProjectDiagnostic {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_ProjectString code;     /* Stable ASCII machine code. */
    uint32_t severity;
    On1xSDK_ProjectString message;  /* Callback-scoped UTF-8. */
    On1xSDK_ProjectSpan primary;
    size_t related_count;
    const On1xSDK_ProjectSpan *related; /* Callback-scoped; bounded by MAX_RELATED. */
} On1xSDK_ProjectDiagnostic;

typedef On1xSDK_Status (*On1xSDK_ProjectDiagnosticFn)(
    void *context, const On1xSDK_ProjectDiagnostic *diagnostic);

typedef struct On1xSDK_ProjectDiagnosticSink {
    uint32_t abi_version;
    uint32_t struct_size;
    void *context;
    On1xSDK_ProjectDiagnosticFn emit;
} On1xSDK_ProjectDiagnosticSink;

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
