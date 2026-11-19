/*
 * Checked access to a bounded function projection from ExoML CDecl v2.
 * Nested descriptors are borrowed and immutable for the enclosing descriptor's
 * lifetime.  Extraction and this structural check do not perform admission.
 */
#ifndef ON1X_SDK_CDECL_H
#define ON1X_SDK_CDECL_H

#include "On1xSDK-ForeignType.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  ON1X_SDK_CDECL_COMPLETE = 1u << 0,
  ON1X_SDK_CDECL_PROTOTYPE = 1u << 1,
  ON1X_SDK_CDECL_VARIADIC = 1u << 2,
  ON1X_SDK_CDECL_HAS_DISCOVERY_ERRORS = 1u << 3
};

/* The admitted subset currently supports only the ordinary target C ABI. */
enum { ON1X_SDK_CDECL_CALL_C = 0u };

typedef struct On1xSDK_CDeclSource {
  uint32_t abi_version;
  uint32_t struct_size;
  uint32_t schema_version;
  uint32_t pointer_bits;
  On1xSDK_FFIText header;
  On1xSDK_FFIText revision;
  On1xSDK_FFIText fingerprint_sha256;
  On1xSDK_FFIText header_sha256;
  On1xSDK_FFIText target;
  On1xSDK_FFIText compiler;
  uint64_t source_size;
  uint32_t flags;
  uint32_t reserved;
} On1xSDK_CDeclSource;

typedef struct On1xSDK_CDeclParameter {
  uint32_t abi_version;
  uint32_t struct_size;
  On1xSDK_FFIText name;
  On1xSDK_ForeignType type;
  On1xSDK_FFISpan source;
} On1xSDK_CDeclParameter;

typedef struct On1xSDK_CDeclFunction {
  uint32_t abi_version;
  uint32_t struct_size;
  const On1xSDK_CDeclSource *source;
  On1xSDK_FFIText c_name;
  On1xSDK_ForeignType result;
  const On1xSDK_CDeclParameter *parameters;
  size_t parameter_count;
  uint32_t calling_convention;
  uint32_t flags;
  On1xSDK_FFISpan declaration_span;
} On1xSDK_CDeclFunction;

On1xSDK_Status on1x_sdk_cdecl_source_check(const On1xSDK_CDeclSource *source);
On1xSDK_Status
on1x_sdk_cdecl_function_check(const On1xSDK_CDeclFunction *function);

On1xSDK_Status
on1x_sdk_cdecl_parameter_count(const On1xSDK_CDeclFunction *function,
                               size_t *count);
On1xSDK_Status
on1x_sdk_cdecl_parameter_at(const On1xSDK_CDeclFunction *function, size_t index,
                            const On1xSDK_CDeclParameter **parameter);

#ifdef __cplusplus
}
#endif

#endif
