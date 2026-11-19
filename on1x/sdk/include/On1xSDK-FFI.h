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

#include "On1xSDK-Types.h"

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

typedef struct On1xSDK_FFIText {
  const char *data;
  size_t size;
} On1xSDK_FFIText;

#ifdef __cplusplus
typedef enum On1xSDK_FFISeverity : uint32_t {
#else
typedef enum On1xSDK_FFISeverity {
#endif
  ON1X_SDK_FFI_NOTE = 1,
  ON1X_SDK_FFI_WARNING = 2,
  ON1X_SDK_FFI_ERROR_SEVERITY = 3
} On1xSDK_FFISeverity;

#ifndef __cplusplus
_Static_assert(sizeof(On1xSDK_FFISeverity) == sizeof(uint32_t),
               "FFI severity requires a 32-bit enum");
#endif

/*
 * Source offsets are zero-based half-open byte offsets.  The revision must
 * match the immutable CDecl source identity.  No line/column surrogate is used
 * as source identity.
 */
typedef struct On1xSDK_FFISpan {
  uint32_t abi_version;
  uint32_t struct_size;
  On1xSDK_FFIText revision;
  uint64_t begin;
  uint64_t end;
  uint64_t source_size;
} On1xSDK_FFISpan;

typedef struct On1xSDK_FFIDiagnostic {
  uint32_t abi_version;
  uint32_t struct_size;
  On1xSDK_FFIText code;
  On1xSDK_FFIText message;
  On1xSDK_FFISeverity severity;
  On1xSDK_FFISpan primary;
  const On1xSDK_FFISpan *related;
  size_t related_count;
} On1xSDK_FFIDiagnostic;

/*
 * The diagnostic is callback-scoped.  Returning a non-OK status is observable
 * and aborts the operation; an unknown numeric callback status becomes ERROR.
 */
typedef On1xSDK_Status (*On1xSDK_FFIDiagnosticSink)(
    void *context, const On1xSDK_FFIDiagnostic *diagnostic);

int on1x_sdk_ffi_text_valid(On1xSDK_FFIText text, int allow_empty);
int on1x_sdk_ffi_sha256_valid(On1xSDK_FFIText text);
int on1x_sdk_ffi_span_valid(const On1xSDK_FFISpan *span,
                            On1xSDK_FFIText expected_revision,
                            uint64_t expected_source_size);

#ifdef __cplusplus
}
#endif

#endif
