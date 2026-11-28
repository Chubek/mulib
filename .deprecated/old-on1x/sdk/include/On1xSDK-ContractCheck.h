/*
 * Structured semantic claims corresponding to ExoML contract v2.
 * A successful shape check means only that claims are complete and consistent
 * with the supplied CDecl projection; it is not review or safe admission.
 */
#ifndef ON1X_SDK_CONTRACT_CHECK_H
#define ON1X_SDK_CONTRACT_CHECK_H

#include "On1xSDK-CDecl.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
typedef enum On1xSDK_ForeignOwnership : uint32_t {
#else
typedef enum On1xSDK_ForeignOwnership {
#endif
  ON1X_SDK_OWNERSHIP_UNKNOWN = 0,
  ON1X_SDK_OWNERSHIP_SCALAR = 1,
  ON1X_SDK_OWNERSHIP_BORROWED_IMMUTABLE = 2,
  ON1X_SDK_OWNERSHIP_BORROWED_MUTABLE = 3,
  ON1X_SDK_OWNERSHIP_OWNED_UNIQUE = 4,
  ON1X_SDK_OWNERSHIP_CONSUMED = 5
} On1xSDK_ForeignOwnership;

#ifdef __cplusplus
typedef enum On1xSDK_ForeignLifetime : uint32_t {
#else
typedef enum On1xSDK_ForeignLifetime {
#endif
  ON1X_SDK_LIFETIME_NONE = 0,
  ON1X_SDK_LIFETIME_CALL = 1,
  ON1X_SDK_LIFETIME_GLOBAL_STATIC = 2,
  ON1X_SDK_LIFETIME_OWNER = 3
} On1xSDK_ForeignLifetime;

#ifdef __cplusplus
typedef enum On1xSDK_ForeignErrorKind : uint32_t {
#else
typedef enum On1xSDK_ForeignErrorKind {
#endif
  ON1X_SDK_FOREIGN_ERROR_NONE = 0,
  ON1X_SDK_FOREIGN_ERROR_NULL_ON_FAILURE = 1,
  ON1X_SDK_FOREIGN_ERROR_BOOLEAN = 2,
  ON1X_SDK_FOREIGN_ERROR_INTEGER_CODE = 3,
  ON1X_SDK_FOREIGN_ERROR_UNSUPPORTED = 255
} On1xSDK_ForeignErrorKind;

#ifdef __cplusplus
typedef enum On1xSDK_ForeignThreadSafety : uint32_t {
#else
typedef enum On1xSDK_ForeignThreadSafety {
#endif
  ON1X_SDK_FOREIGN_SINGLE_THREAD = 1,
  ON1X_SDK_FOREIGN_THREAD_SAFE = 2,
  ON1X_SDK_FOREIGN_THREAD_AFFINE = 3,
  ON1X_SDK_FOREIGN_THREAD_UNKNOWN = 255
} On1xSDK_ForeignThreadSafety;

#ifndef __cplusplus
_Static_assert(sizeof(On1xSDK_ForeignOwnership) == sizeof(uint32_t),
               "foreign ownership requires a 32-bit enum");
_Static_assert(sizeof(On1xSDK_ForeignLifetime) == sizeof(uint32_t),
               "foreign lifetime requires a 32-bit enum");
_Static_assert(sizeof(On1xSDK_ForeignErrorKind) == sizeof(uint32_t),
               "foreign error kind requires a 32-bit enum");
_Static_assert(sizeof(On1xSDK_ForeignThreadSafety) == sizeof(uint32_t),
               "foreign thread safety requires a 32-bit enum");
#endif

enum {
  ON1X_SDK_FOREIGN_EFFECT_READ = 1u << 0,
  ON1X_SDK_FOREIGN_EFFECT_WRITE = 1u << 1,
  ON1X_SDK_FOREIGN_EFFECT_ALLOCATE = 1u << 2,
  ON1X_SDK_FOREIGN_EFFECT_FREE = 1u << 3,
  ON1X_SDK_FOREIGN_EFFECT_ESCAPE = 1u << 4,
  ON1X_SDK_FOREIGN_EFFECT_IO = 1u << 5,
  ON1X_SDK_FOREIGN_EFFECT_THREAD = 1u << 6,
  ON1X_SDK_FOREIGN_EFFECT_SUSPEND = 1u << 7,
  ON1X_SDK_FOREIGN_EFFECT_FOREIGN_CALL = 1u << 8,
  ON1X_SDK_FOREIGN_EFFECT_CALLBACK = 1u << 9
};

typedef struct On1xSDK_ForeignParameterClaim {
  uint32_t abi_version;
  uint32_t struct_size;
  uint32_t index;
  On1xSDK_ForeignOwnership ownership;
  On1xSDK_ForeignLifetime lifetime;
  uint32_t nullable;
  uint32_t retained;
  On1xSDK_FFIText owner_parameter;
} On1xSDK_ForeignParameterClaim;

typedef struct On1xSDK_ForeignReturnClaim {
  uint32_t abi_version;
  uint32_t struct_size;
  On1xSDK_ForeignOwnership ownership;
  On1xSDK_ForeignLifetime lifetime;
  uint32_t nullable;
  uint32_t reserved;
  On1xSDK_FFIText resource_id;
  On1xSDK_FFIText destroy_with;
} On1xSDK_ForeignReturnClaim;

typedef struct On1xSDK_ForeignContractClaim {
  uint32_t abi_version;
  uint32_t struct_size;
  uint32_t schema_version;
  uint32_t effects_known;
  On1xSDK_FFIText function_name;
  On1xSDK_FFIText cdecl_fingerprint_sha256;
  On1xSDK_FFIText contract_fingerprint_sha256;
  On1xSDK_FFIText source_revision;
  const On1xSDK_ForeignParameterClaim *parameters;
  size_t parameter_count;
  On1xSDK_ForeignReturnClaim result;
  uint32_t effects;
  On1xSDK_ForeignErrorKind error_kind;
  On1xSDK_ForeignThreadSafety thread_safety;
  uint32_t may_block;
  uint32_t may_reenter;
} On1xSDK_ForeignContractClaim;

On1xSDK_Status
on1x_sdk_contract_claim_check(const On1xSDK_CDeclFunction *function,
                              const On1xSDK_ForeignContractClaim *claim);

#ifdef __cplusplus
}
#endif

#endif
