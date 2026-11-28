/* ABI compatibility for the retired scalar descriptor registry. */
#ifndef ON1X_SDK_FFI_DESCRIPTOR_H
#define ON1X_SDK_FFI_DESCRIPTOR_H

#include "On1xSDK-FFI.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_FFI_DESCRIPTOR_SCHEMA_V2 2u
#define ON1X_SDK_FFI_DESCRIPTOR_MAX_PARAMETERS 16u
#define ON1X_SDK_FFI_DESCRIPTOR_MAX_GUARDS 16u

typedef struct On1xSDK_FFIDescriptorQuery {
  uint32_t abi_version;
  uint32_t struct_size;
  uint32_t descriptor_schema;
  On1xSDK_FFIText target; /* Exact canonical triple, no default. */
  On1xSDK_FFIText module; /* Qualified name, e.g. "mulib.clamp". */
  On1xSDK_FFIText member;
  /* Optional lowercase SHA-256 pin. If both names are empty, lookup by this ID
   * instead. An ID identifies a descriptor, not a native artifact or review. */
  On1xSDK_FFIText descriptor_id;
} On1xSDK_FFIDescriptorQuery;

enum {
  ON1X_SDK_FFI_SURFACE_VOID = 1,
  ON1X_SDK_FFI_SURFACE_I64,
  ON1X_SDK_FFI_SURFACE_U8,
  ON1X_SDK_FFI_SURFACE_U16,
  ON1X_SDK_FFI_SURFACE_U32,
  ON1X_SDK_FFI_SURFACE_U64,
  ON1X_SDK_FFI_SURFACE_F32,
  ON1X_SDK_FFI_SURFACE_F64,
  ON1X_SDK_FFI_SURFACE_BOOL
};

enum {
  ON1X_SDK_FFI_NATIVE_VOID = 1,
  ON1X_SDK_FFI_NATIVE_INTEGER,
  ON1X_SDK_FFI_NATIVE_REAL
};

enum {
  ON1X_SDK_FFI_PLACEMENT_EXTERNAL = 1,
  ON1X_SDK_FFI_PLACEMENT_STATIC_INLINE
};

/* Fixed schema-v2 subrecords, not independently extensible. Native primitive
 * identity is kept separate from both layout and the On1x surface type. */
typedef struct On1xSDK_FFIScalarTypeV2 {
  uint32_t surface_kind;
  uint32_t native_kind;
  On1xSDK_FFIText native_identity;
  On1xSDK_FFIText native_format;
  uint16_t native_rank;
  uint16_t native_bits;
  uint16_t native_size_bytes;
  uint16_t native_alignment_bytes;
  uint32_t native_is_signed;
} On1xSDK_FFIScalarTypeV2;

typedef struct On1xSDK_FFIScalarParameterV2 {
  On1xSDK_FFIText name;
  On1xSDK_FFIScalarTypeV2 type;
} On1xSDK_FFIScalarParameterV2;

typedef struct On1xSDK_FFILeGuardV2 {
  uint8_t lhs;
  uint8_t rhs;
} On1xSDK_FFILeGuardV2;

typedef struct On1xSDK_FFIDescriptorView {
  uint32_t abi_version;
  uint32_t struct_size;
  uint32_t descriptor_schema;
  uint32_t placement;
  On1xSDK_FFIText module;
  On1xSDK_FFIText member;
  On1xSDK_FFIText provider;
  On1xSDK_FFIText header;
  On1xSDK_FFIText native_symbol;
  On1xSDK_FFIText native_definition;
  On1xSDK_FFIText target;
  On1xSDK_FFIText data_model;
  On1xSDK_FFIText native_abi;
  On1xSDK_FFIText calling_convention;
  On1xSDK_FFIText descriptor_id;
  On1xSDK_FFIText native_fingerprint;
  On1xSDK_FFIScalarTypeV2 result;
  uint32_t parameter_count;
  uint32_t guard_count;
  On1xSDK_FFIScalarParameterV2 parameters[ON1X_SDK_FFI_DESCRIPTOR_MAX_PARAMETERS];
  On1xSDK_FFILeGuardV2 guards[ON1X_SDK_FFI_DESCRIPTOR_MAX_GUARDS];
} On1xSDK_FFIDescriptorView;

/*
 * Both records must use SDK ABI 2 and cover their full known structure.
 * Names and target are bounded UTF-8 without embedded NULs; matching is
 * byte-exact. The maximum length is ON1X_SDK_FFI_MAX_TEXT_BYTES.
 * A valid output is cleared (except its prefix) on failure. It must not
 * overlap the query or its backing strings. Valid queries return
 * ON1X_SDK_UNSUPPORTED: the v2 registry has been retired.
 */
On1xSDK_Status
on1x_sdk_ffi_descriptor_find(const On1xSDK_FFIDescriptorQuery *query,
                            On1xSDK_FFIDescriptorView *out);

#ifdef __cplusplus
}
#endif
#endif
