/*
 * Host-authorized review and executable-boundary evidence.
 * Review fields are claims until both callbacks in an admission policy accept
 * the exact subject.  Requested review/capabilities never grant authority.
 */
#ifndef ON1X_SDK_CONTRACT_REVIEW_H
#define ON1X_SDK_CONTRACT_REVIEW_H

#include "On1xSDK-ContractCheck.h"
#include "On1xSDK-Exotract.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_ForeignEvidence {
  uint32_t abi_version;
  uint32_t struct_size;
  uint32_t schema_version;
  uint32_t reserved;
  On1xSDK_FFIText subject_sha256;
  On1xSDK_FFIText reviewer;
  On1xSDK_FFIText review_reference;
  On1xSDK_FFIText review_sha256;
  On1xSDK_FFIText abi_probe_sha256;
  On1xSDK_FFIText boundary_test_sha256;
  On1xSDK_FFIText target;
  uint32_t boundary_test_count;
  uint32_t sanitizer_flags;
} On1xSDK_ForeignEvidence;

typedef struct On1xSDK_ForeignCandidate On1xSDK_ForeignCandidate;

/*
 * Callbacks are synchronous, receive borrowed immutable data, and may not
 * retain it, throw through C, or recursively invoke admission.
 */
typedef On1xSDK_Status (*On1xSDK_ForeignReviewCallback)(
    void *context, const On1xSDK_ForeignCandidate *candidate,
    const On1xSDK_ForeignEvidence *evidence);
typedef On1xSDK_Status (*On1xSDK_ForeignBoundaryCallback)(
    void *context, const On1xSDK_ForeignCandidate *candidate,
    const On1xSDK_ForeignEvidence *evidence);

typedef struct On1xSDK_ForeignAdmissionPolicy {
  uint32_t abi_version;
  uint32_t struct_size;
  On1xSDK_FFIText target;
  uint32_t pointer_bits;
  uint32_t calling_convention;
  uint32_t max_parameters;
  uint32_t allowed_effects;
  void *callback_context;
  On1xSDK_ForeignReviewCallback authorize_review;
  On1xSDK_ForeignBoundaryCallback verify_boundary;
  void *diagnostic_context;
  On1xSDK_FFIDiagnosticSink diagnostic_sink;
} On1xSDK_ForeignAdmissionPolicy;

On1xSDK_Status
on1x_sdk_contract_review_check(const On1xSDK_ForeignEvidence *evidence,
                               const On1xSDK_ForeignAdmissionPolicy *policy);

#ifdef __cplusplus
}
#endif

#endif
