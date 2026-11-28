#ifndef ON1X_SDK_DIAGNOSTIC_H
#define ON1X_SDK_DIAGNOSTIC_H

#include "On1xSDK-Span.h"
#include "On1xSDK-String.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_Severity;
enum {
    ON1X_SDK_NOTE = 1,
    ON1X_SDK_WARNING = 2,
    ON1X_SDK_DIAGNOSTIC_ERROR = 3
};

typedef struct {
    On1xSDK_Severity severity;
    const char *code;    /* NUL-terminated machine-readable identifier. */
    const char *message; /* NUL-terminated human-readable text. */
    /* Zero-based, half-open byte offsets; both SIZE_MAX if unlocated. */
    size_t byte_begin;
    size_t byte_end;
} On1xSDK_Diagnostic;

/*
 * Called synchronously; the record and its strings are borrowed for the
 * call. Do not throw or retain them.
 */
typedef void (*On1xSDK_DiagnosticFn)(void *userdata, const On1xSDK_Diagnostic *diagnostic);

typedef struct {
    On1xSDK_DiagnosticFn emit; /* NULL discards diagnostics. */
    void *userdata;           /* Borrowed, never freed by the SDK. */
} On1xSDK_DiagnosticSink;

#define ON1X_SDK_MAX_RELATED_DIAGNOSTICS 16u
#define ON1X_SDK_MAX_FIXITS 8u
#define ON1X_SDK_MAX_DIAGNOSTIC_TEXT_BYTES (16u * 1024u)

typedef struct On1xSDK_RelatedDiagnostic {
    On1xSDK_SourceSpan span;
    On1xSDK_StringView message;
} On1xSDK_RelatedDiagnostic;

typedef struct On1xSDK_FixIt {
    On1xSDK_SourceSpan span;
    On1xSDK_StringView replacement;
    uint32_t is_proposal;
    uint32_t reserved;
} On1xSDK_FixIt;

typedef struct On1xSDK_DiagnosticV2 {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_Severity severity;
    uint32_t reserved;
    On1xSDK_StringView code;
    On1xSDK_StringView message;
    On1xSDK_SourceSpan primary;
    const On1xSDK_RelatedDiagnostic *related;
    size_t related_count;
    const On1xSDK_FixIt *fixits;
    size_t fixit_count;
    On1xSDK_StringView category;
    On1xSDK_StringView phase;
    On1xSDK_StringView provider;
} On1xSDK_DiagnosticV2;

typedef On1xSDK_Status (*On1xSDK_DiagnosticFnV2)(
    void *userdata, const On1xSDK_DiagnosticV2 *diagnostic
);

typedef struct On1xSDK_DiagnosticSinkV2 {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_DiagnosticFnV2 emit;
    void *userdata;
} On1xSDK_DiagnosticSinkV2;

On1xSDK_Status on1x_sdk_diagnostic_validate(
    const On1xSDK_DiagnosticV2 *diagnostic,
    const On1xSDK_SourceRevision *source
);
On1xSDK_Status on1x_sdk_diagnostic_emit_v2(
    const On1xSDK_DiagnosticSinkV2 *sink,
    const On1xSDK_DiagnosticV2 *diagnostic,
    const On1xSDK_SourceRevision *source
);
/* Explicit lossy compatibility adapter for the ABI-v1 void sink. */
On1xSDK_Status on1x_sdk_diagnostic_emit_v1(
    const On1xSDK_DiagnosticSink *sink,
    const On1xSDK_DiagnosticV2 *diagnostic,
    const On1xSDK_SourceRevision *source
);

#ifdef __cplusplus
}
#endif

#endif
