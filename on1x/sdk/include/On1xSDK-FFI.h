/*
 * Structured foreign-interface projection for SDK ABI 2.
 *
 * This API describes a checked projection of canonical ExoML CDecl/contract
 * data.  It is not a C parser, a contract authority, or an executable symbol
 * resolver.  All input and output views are borrowed for the duration stated by
 * the operation; this module allocates no cross-boundary memory.
 */
#ifndef ON1X_SDK_FFI_H
#define ON1X_SDK_FFI_H

#include "On1xSDK-Diagnostic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_FFI_ABI_VERSION 2u
#define ON1X_SDK_FFI_CDECL_SCHEMA_VERSION 2u
#define ON1X_SDK_FFI_CONTRACT_SCHEMA_VERSION 2u
#define ON1X_SDK_FFI_ADMISSION_SCHEMA_VERSION 1u

#define ON1X_SDK_FFI_MAX_PARAMETERS 32u
#define ON1X_SDK_FFI_MAX_RELATED_DIAGNOSTICS 8u
#define ON1X_SDK_FFI_MAX_TEXT_BYTES 4096u
#define ON1X_SDK_FFI_SHA256_HEX_BYTES 64u

/* Source-compatible aliases over the shared ABI-2 text contract. */
typedef On1xSDK_StringView On1xSDK_FFIText;
typedef On1xSDK_Severity On1xSDK_FFISeverity;
enum {
  ON1X_SDK_FFI_NOTE = ON1X_SDK_NOTE,
  ON1X_SDK_FFI_WARNING = ON1X_SDK_WARNING,
  ON1X_SDK_FFI_ERROR_SEVERITY = ON1X_SDK_DIAGNOSTIC_ERROR
};

/*
 * Source offsets are zero-based half-open byte offsets.  The revision must
 * match the immutable CDecl source identity.  No line/column surrogate is used
 * as source identity.
 */
typedef On1xSDK_SourceSpan On1xSDK_FFISpan;
typedef On1xSDK_DiagnosticV2 On1xSDK_FFIDiagnostic;

/*
 * The diagnostic is callback-scoped.  Returning a non-OK status is observable
 * and aborts the operation; an unknown numeric callback status becomes ERROR.
 */
typedef On1xSDK_DiagnosticFnV2 On1xSDK_FFIDiagnosticSink;

int on1x_sdk_ffi_text_valid(On1xSDK_FFIText text, int allow_empty);
int on1x_sdk_ffi_sha256_valid(On1xSDK_FFIText text);
int on1x_sdk_ffi_span_valid(const On1xSDK_FFISpan *span,
                            const On1xSDK_SourceRevision *source);

#ifdef __cplusplus
}
#endif

#endif
