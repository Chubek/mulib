/* Exact, target-qualified foreign type projection. */
#ifndef ON1X_SDK_FOREIGN_TYPE_H
#define ON1X_SDK_FOREIGN_TYPE_H

#include "On1xSDK-FFI.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
typedef enum On1xSDK_ForeignTypeKind : uint32_t {
#else
typedef enum On1xSDK_ForeignTypeKind {
#endif
  ON1X_SDK_FOREIGN_VOID = 1,
  ON1X_SDK_FOREIGN_BOOL = 2,
  ON1X_SDK_FOREIGN_SIGNED_INTEGER = 3,
  ON1X_SDK_FOREIGN_UNSIGNED_INTEGER = 4,
  ON1X_SDK_FOREIGN_BINARY32 = 5,
  ON1X_SDK_FOREIGN_BINARY64 = 6,
  ON1X_SDK_FOREIGN_C_STRING = 7,
  ON1X_SDK_FOREIGN_OPAQUE_HANDLE = 8,
  ON1X_SDK_FOREIGN_UNSUPPORTED = 255
} On1xSDK_ForeignTypeKind;

#ifndef __cplusplus
_Static_assert(sizeof(On1xSDK_ForeignTypeKind) == sizeof(uint32_t),
               "foreign type kind requires a 32-bit enum");
#endif

enum {
  ON1X_SDK_FOREIGN_TYPE_POINTER = 1u << 0,
  ON1X_SDK_FOREIGN_TYPE_CONST_POINTEE = 1u << 1,
  ON1X_SDK_FOREIGN_TYPE_VOLATILE = 1u << 2,
  ON1X_SDK_FOREIGN_TYPE_RESTRICT = 1u << 3,
  ON1X_SDK_FOREIGN_TYPE_IEEE754 = 1u << 4,
  ON1X_SDK_FOREIGN_TYPE_NUL_TERMINATED = 1u << 5,
  ON1X_SDK_FOREIGN_TYPE_COMPLETE_LAYOUT = 1u << 6
};

/*
 * canonical_id is the canonical ExoML CDecl type ID in the named translation
 * unit.  nominal_id is required only for an opaque handle.  Opaque pointee
 * layout is intentionally hidden; the pointer layout itself remains exact.
 */
typedef struct On1xSDK_ForeignType {
  uint32_t abi_version;
  uint32_t struct_size;
  On1xSDK_ForeignTypeKind kind;
  uint32_t flags;
  On1xSDK_FFIText canonical_id;
  On1xSDK_FFIText nominal_id;
  uint32_t size_bytes;
  uint32_t alignment_bytes;
  uint32_t bit_width;
  uint32_t reserved;
} On1xSDK_ForeignType;

On1xSDK_Status on1x_sdk_foreign_type_check(const On1xSDK_ForeignType *type,
                                           uint32_t target_pointer_bits);

#ifdef __cplusplus
}
#endif

#endif
