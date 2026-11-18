#ifndef ON1X_SDK_DIAGNOSTIC_H
#define ON1X_SDK_DIAGNOSTIC_H

#include "On1xSDK-Types.h"

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

#ifdef __cplusplus
}
#endif

#endif
