/*
 * Final host-owned foreign-admission gate for the deliberately narrow SDK
 * profile.  It does not publish compiler state or resolve a C symbol.
 */
#ifndef ON1X_SDK_KERNEL_BIND_H
#define ON1X_SDK_KERNEL_BIND_H

#include "On1xSDK-Contract.h"
#include "On1xSDK-ContractReview.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
typedef enum On1xSDK_ForeignProfile : uint32_t {
#else
typedef enum On1xSDK_ForeignProfile {
#endif
  ON1X_SDK_FOREIGN_PROFILE_SCALAR = 1,
  ON1X_SDK_FOREIGN_PROFILE_IMMUTABLE_C_STRING = 2,
  ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_CREATE = 3,
  ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_BORROW = 4,
  ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_DESTROY = 5
} On1xSDK_ForeignProfile;

#ifdef __cplusplus
typedef enum On1xSDK_ForeignAdmissionCode : uint32_t {
#else
typedef enum On1xSDK_ForeignAdmissionCode {
#endif
  ON1X_SDK_ADMISSION_NONE = 0,
  ON1X_SDK_ADMISSION_ADMITTED = 1,
  ON1X_SDK_ADMISSION_INVALID_DESCRIPTOR = 2,
  ON1X_SDK_ADMISSION_ABI_VERSION = 3,
  ON1X_SDK_ADMISSION_TARGET_MISMATCH = 4,
  ON1X_SDK_ADMISSION_SOURCE_DRIFT = 5,
  ON1X_SDK_ADMISSION_CONTRACT_DRIFT = 6,
  ON1X_SDK_ADMISSION_UNKNOWN_LAYOUT = 7,
  ON1X_SDK_ADMISSION_UNSUPPORTED_TYPE = 8,
  ON1X_SDK_ADMISSION_UNKNOWN_OWNERSHIP = 9,
  ON1X_SDK_ADMISSION_UNKNOWN_EFFECTS = 10,
  ON1X_SDK_ADMISSION_UNSUPPORTED_EFFECT = 11,
  ON1X_SDK_ADMISSION_NULLABILITY = 12,
  ON1X_SDK_ADMISSION_LIFETIME = 13,
  ON1X_SDK_ADMISSION_REVIEW_REQUIRED = 14,
  ON1X_SDK_ADMISSION_BOUNDARY_REQUIRED = 15,
  ON1X_SDK_ADMISSION_CALLBACK_FAILED = 16,
  ON1X_SDK_ADMISSION_DIAGNOSTIC_FAILED = 17,
  ON1X_SDK_ADMISSION_DISCOVERY_ONLY = 18,
  ON1X_SDK_ADMISSION_ONE_TU_REQUIRED = 19,
  ON1X_SDK_ADMISSION_LEGACY_METADATA_ONLY = 20
} On1xSDK_ForeignAdmissionCode;

#ifndef __cplusplus
_Static_assert(sizeof(On1xSDK_ForeignProfile) == sizeof(uint32_t),
               "foreign profile requires a 32-bit enum");
_Static_assert(sizeof(On1xSDK_ForeignAdmissionCode) == sizeof(uint32_t),
               "foreign admission code requires a 32-bit enum");
#endif

struct On1xSDK_ForeignCandidate {
  uint32_t abi_version;
  uint32_t struct_size;
  uint32_t admission_schema_version;
  On1xSDK_ForeignProfile profile;
  const On1xSDK_CDeclFunction *declaration;
  const On1xSDK_ForeignContractClaim *contract;
  const On1xSDK_ExotractProfile *exotract;
  const On1xSDK_ForeignEvidence *evidence;
};

typedef struct On1xSDK_ForeignAdmissionResult {
  uint32_t abi_version;
  uint32_t struct_size;
  uint32_t admitted;
  On1xSDK_ForeignAdmissionCode code;
  On1xSDK_FFIText machine_code;
} On1xSDK_ForeignAdmissionResult;

/*
 * OK means admitted.  Unsupported/malformed/callback statuses remain distinct,
 * and result always receives a stable machine code when its prefix is valid.
 */
On1xSDK_Status
on1x_sdk_foreign_admit(const On1xSDK_ForeignCandidate *candidate,
                       const On1xSDK_ForeignAdmissionPolicy *policy,
                       On1xSDK_ForeignAdmissionResult *result);

/*
 * Explicit ABI-v1 adapter.  The legacy descriptor remains inspectable metadata;
 * its opaque signature/ownership strings cannot become structured admission.
 */
On1xSDK_Status
on1x_sdk_foreign_admit_v1_contract(const On1xSDK_Contract *legacy,
                                   On1xSDK_ForeignAdmissionResult *result);

const char *
on1x_sdk_foreign_admission_code_name(On1xSDK_ForeignAdmissionCode code);

#ifdef __cplusplus
}
#endif

#endif
