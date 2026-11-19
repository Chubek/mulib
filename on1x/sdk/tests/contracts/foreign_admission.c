#include "On1xSDK-BindingGenerator.h"
#include "On1xSDK-CDecl.h"
#include "On1xSDK-ContractCheck.h"
#include "On1xSDK-ContractReview.h"
#include "On1xSDK-ExoLib.h"
#include "On1xSDK-ExoML.h"
#include "On1xSDK-Exotract.h"
#include "On1xSDK-FFI.h"
#include "On1xSDK-ForeignCallback.h"
#include "On1xSDK-ForeignModule.h"
#include "On1xSDK-ForeignResource.h"
#include "On1xSDK-ForeignType.h"
#include "On1xSDK-KernelBind.h"
#include "On1xSDK-KernelDiscovery.h"
#include "On1xSDK-Marshalling.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEXT(text) ((On1xSDK_FFIText){text, sizeof(text) - 1u})

static int failure(int line, const char *message) {
  fprintf(stderr, "foreign_admission.c:%d: %s\n", line, message);
  return line;
}

#define CHECK(condition, message)                                              \
  do {                                                                         \
    if (!(condition))                                                          \
      return failure(__LINE__, message);                                       \
  } while (0)

typedef struct CallbackState {
  unsigned reviews;
  unsigned boundaries;
  unsigned diagnostics;
  On1xSDK_Status review_status;
  On1xSDK_Status boundary_status;
  On1xSDK_Status diagnostic_status;
  int reenter;
  On1xSDK_Status nested_status;
  const On1xSDK_ForeignAdmissionPolicy *policy;
} CallbackState;

static On1xSDK_Status
authorize_review(void *opaque, const On1xSDK_ForeignCandidate *candidate,
                 const On1xSDK_ForeignEvidence *evidence) {
  CallbackState *state = (CallbackState *)opaque;
  ++state->reviews;
  if (!candidate || !evidence)
    return ON1X_SDK_INVALID_ARGUMENT;
  if (state->reenter) {
    On1xSDK_ForeignAdmissionResult nested = {
        ON1X_SDK_FFI_ABI_VERSION,
        sizeof(On1xSDK_ForeignAdmissionResult),
        0u,
        ON1X_SDK_ADMISSION_NONE,
        {NULL, 0u}};
    state->reenter = 0;
    state->nested_status =
        on1x_sdk_foreign_admit(candidate, state->policy, &nested);
    if (nested.code != ON1X_SDK_ADMISSION_CALLBACK_FAILED)
      return ON1X_SDK_ERROR;
  }
  return state->review_status;
}

static On1xSDK_Status verify_boundary(void *opaque,
                                      const On1xSDK_ForeignCandidate *candidate,
                                      const On1xSDK_ForeignEvidence *evidence) {
  CallbackState *state = (CallbackState *)opaque;
  ++state->boundaries;
  if (!candidate || !evidence)
    return ON1X_SDK_INVALID_ARGUMENT;
  return state->boundary_status;
}

static On1xSDK_Status
receive_diagnostic(void *opaque, const On1xSDK_FFIDiagnostic *diagnostic) {
  CallbackState *state = (CallbackState *)opaque;
  ++state->diagnostics;
  if (!diagnostic || diagnostic->abi_version != ON1X_SDK_FFI_ABI_VERSION ||
      !diagnostic->code.size || !diagnostic->message.size)
    return ON1X_SDK_INVALID_ARGUMENT;
  return state->diagnostic_status;
}

typedef struct ScalarFixture {
  On1xSDK_CDeclSource source;
  On1xSDK_CDeclParameter parameters[2];
  On1xSDK_CDeclFunction function;
  On1xSDK_ForeignParameterClaim parameter_claims[2];
  On1xSDK_ForeignContractClaim contract;
  On1xSDK_FFIText switches[1];
  On1xSDK_ExotractProfile exotract;
  On1xSDK_ForeignEvidence evidence;
  On1xSDK_ForeignAdmissionPolicy policy;
  On1xSDK_ForeignCandidate candidate;
  CallbackState callbacks;
} ScalarFixture;

static On1xSDK_FFISpan span(uint64_t begin, uint64_t end) {
  On1xSDK_FFISpan value = {ON1X_SDK_FFI_ABI_VERSION,
                           sizeof(On1xSDK_FFISpan),
                           TEXT("revision-1"),
                           begin,
                           end,
                           100u};
  return value;
}

static On1xSDK_ForeignType integer_type(const char *id, size_t id_size) {
  On1xSDK_ForeignType type;
  memset(&type, 0, sizeof(type));
  type.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  type.struct_size = sizeof(type);
  type.kind = ON1X_SDK_FOREIGN_SIGNED_INTEGER;
  type.flags = ON1X_SDK_FOREIGN_TYPE_COMPLETE_LAYOUT;
  type.canonical_id = (On1xSDK_FFIText){id, id_size};
  type.size_bytes = 8u;
  type.alignment_bytes = 8u;
  type.bit_width = 64u;
  return type;
}

static On1xSDK_ForeignType void_type(void) {
  On1xSDK_ForeignType type;
  memset(&type, 0, sizeof(type));
  type.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  type.struct_size = sizeof(type);
  type.kind = ON1X_SDK_FOREIGN_VOID;
  type.canonical_id = TEXT("void");
  return type;
}

static void initialize_fixture(ScalarFixture *fixture) {
  static const char cdecl_hash[] =
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  static const char contract_hash[] =
      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
  static const char review_hash[] =
      "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc";
  static const char abi_hash[] =
      "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd";
  static const char boundary_hash[] =
      "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee";
  memset(fixture, 0, sizeof(*fixture));
  fixture->source.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture->source.struct_size = sizeof(fixture->source);
  fixture->source.schema_version = ON1X_SDK_FFI_CDECL_SCHEMA_VERSION;
  fixture->source.pointer_bits = 64u;
  fixture->source.header = TEXT("fixture.h");
  fixture->source.revision = TEXT("revision-1");
  fixture->source.fingerprint_sha256 = (On1xSDK_FFIText){cdecl_hash, 64u};
  fixture->source.header_sha256 =
      TEXT("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
  fixture->source.target = TEXT("x86_64-linux-gnu");
  fixture->source.compiler = TEXT("clang-22");
  fixture->source.source_size = 100u;
  fixture->source.flags = ON1X_SDK_CDECL_COMPLETE;
  for (size_t index = 0; index < 2u; ++index) {
    fixture->parameters[index].abi_version = ON1X_SDK_FFI_ABI_VERSION;
    fixture->parameters[index].struct_size = sizeof(On1xSDK_CDeclParameter);
    fixture->parameters[index].name = index ? TEXT("right") : TEXT("left");
    fixture->parameters[index].type =
        integer_type(index ? "i64-r" : "i64-l", 5u);
    fixture->parameters[index].source =
        span(10u + index * 10u, 15u + index * 10u);
    fixture->parameter_claims[index].abi_version = ON1X_SDK_FFI_ABI_VERSION;
    fixture->parameter_claims[index].struct_size =
        sizeof(On1xSDK_ForeignParameterClaim);
    fixture->parameter_claims[index].index = (uint32_t)index;
    fixture->parameter_claims[index].ownership = ON1X_SDK_OWNERSHIP_SCALAR;
    fixture->parameter_claims[index].lifetime = ON1X_SDK_LIFETIME_NONE;
  }
  fixture->function.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture->function.struct_size = sizeof(fixture->function);
  fixture->function.source = &fixture->source;
  fixture->function.c_name = TEXT("scalar_add");
  fixture->function.result = integer_type("i64-result", 10u);
  fixture->function.parameters = fixture->parameters;
  fixture->function.parameter_count = 2u;
  fixture->function.calling_convention = ON1X_SDK_CDECL_CALL_C;
  fixture->function.flags = ON1X_SDK_CDECL_PROTOTYPE;
  fixture->function.declaration_span = span(0u, 40u);
  fixture->contract.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture->contract.struct_size = sizeof(fixture->contract);
  fixture->contract.schema_version = ON1X_SDK_FFI_CONTRACT_SCHEMA_VERSION;
  fixture->contract.effects_known = 1u;
  fixture->contract.function_name = fixture->function.c_name;
  fixture->contract.cdecl_fingerprint_sha256 =
      (On1xSDK_FFIText){cdecl_hash, 64u};
  fixture->contract.contract_fingerprint_sha256 =
      (On1xSDK_FFIText){contract_hash, 64u};
  fixture->contract.source_revision = fixture->source.revision;
  fixture->contract.parameters = fixture->parameter_claims;
  fixture->contract.parameter_count = 2u;
  fixture->contract.result.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture->contract.result.struct_size = sizeof(On1xSDK_ForeignReturnClaim);
  fixture->contract.result.ownership = ON1X_SDK_OWNERSHIP_SCALAR;
  fixture->contract.result.lifetime = ON1X_SDK_LIFETIME_NONE;
  fixture->contract.error_kind = ON1X_SDK_FOREIGN_ERROR_NONE;
  fixture->contract.thread_safety = ON1X_SDK_FOREIGN_THREAD_SAFE;
  fixture->switches[0] = TEXT("MULIB_IMPL_FIXTURE");
  fixture->exotract.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture->exotract.struct_size = sizeof(fixture->exotract);
  fixture->exotract.schema_version = ON1X_SDK_EXOTRACT_SCHEMA_VERSION;
  fixture->exotract.flags = ON1X_SDK_EXOTRACT_DISCOVERY_COMPLETE |
                            ON1X_SDK_EXOTRACT_SYNTAX_CHECKED |
                            ON1X_SDK_EXOTRACT_DECLARATION_ONLY;
  fixture->exotract.library = TEXT("fixture");
  fixture->exotract.manifest_sha256 =
      TEXT("1111111111111111111111111111111111111111111111111111111111111111");
  fixture->exotract.profile_sha256 =
      TEXT("2222222222222222222222222222222222222222222222222222222222222222");
  fixture->exotract.implementation_switches = fixture->switches;
  fixture->exotract.implementation_switch_count = 1u;
  fixture->exotract.implementation_translation_units = 1u;
  fixture->evidence.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture->evidence.struct_size = sizeof(fixture->evidence);
  fixture->evidence.schema_version = ON1X_SDK_FFI_ADMISSION_SCHEMA_VERSION;
  fixture->evidence.subject_sha256 = (On1xSDK_FFIText){contract_hash, 64u};
  fixture->evidence.reviewer = TEXT("host-reviewer");
  fixture->evidence.review_reference = TEXT("review/42");
  fixture->evidence.review_sha256 = (On1xSDK_FFIText){review_hash, 64u};
  fixture->evidence.abi_probe_sha256 = (On1xSDK_FFIText){abi_hash, 64u};
  fixture->evidence.boundary_test_sha256 =
      (On1xSDK_FFIText){boundary_hash, 64u};
  fixture->evidence.target = fixture->source.target;
  fixture->evidence.boundary_test_count = 12u;
  fixture->evidence.sanitizer_flags = 3u;
  fixture->callbacks.review_status = ON1X_SDK_OK;
  fixture->callbacks.boundary_status = ON1X_SDK_OK;
  fixture->callbacks.diagnostic_status = ON1X_SDK_OK;
  fixture->policy.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture->policy.struct_size = sizeof(fixture->policy);
  fixture->policy.target = fixture->source.target;
  fixture->policy.pointer_bits = 64u;
  fixture->policy.calling_convention = ON1X_SDK_CDECL_CALL_C;
  fixture->policy.max_parameters = ON1X_SDK_FFI_MAX_PARAMETERS;
  fixture->policy.allowed_effects = ON1X_SDK_FOREIGN_EFFECT_READ |
                                    ON1X_SDK_FOREIGN_EFFECT_ALLOCATE |
                                    ON1X_SDK_FOREIGN_EFFECT_FREE;
  fixture->policy.callback_context = &fixture->callbacks;
  fixture->policy.authorize_review = authorize_review;
  fixture->policy.verify_boundary = verify_boundary;
  fixture->policy.diagnostic_context = &fixture->callbacks;
  fixture->policy.diagnostic_sink = receive_diagnostic;
  fixture->callbacks.policy = &fixture->policy;
  fixture->candidate.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture->candidate.struct_size = sizeof(fixture->candidate);
  fixture->candidate.admission_schema_version =
      ON1X_SDK_FFI_ADMISSION_SCHEMA_VERSION;
  fixture->candidate.profile = ON1X_SDK_FOREIGN_PROFILE_SCALAR;
  fixture->candidate.declaration = &fixture->function;
  fixture->candidate.contract = &fixture->contract;
  fixture->candidate.exotract = &fixture->exotract;
  fixture->candidate.evidence = &fixture->evidence;
}

static On1xSDK_ForeignAdmissionResult result(void) {
  const On1xSDK_ForeignAdmissionResult value = {
      ON1X_SDK_FFI_ABI_VERSION,
      sizeof(On1xSDK_ForeignAdmissionResult),
      0u,
      ON1X_SDK_ADMISSION_NONE,
      {NULL, 0u}};
  return value;
}

static int test_scalar_and_accessors(void) {
  ScalarFixture fixture;
  On1xSDK_ForeignAdmissionResult admission = result();
  const On1xSDK_CDeclParameter *parameter = NULL;
  size_t count = 0u;
  On1xSDK_FFIText implementation_switch = {NULL, 0u};
  initialize_fixture(&fixture);
  CHECK(on1x_sdk_cdecl_function_check(&fixture.function) == ON1X_SDK_OK,
        "valid CDecl projection rejected");
  CHECK(on1x_sdk_contract_claim_check(&fixture.function, &fixture.contract) ==
            ON1X_SDK_OK,
        "valid contract projection rejected");
  CHECK(on1x_sdk_exoml_projection_check(&fixture.function, &fixture.contract) ==
            ON1X_SDK_OK,
        "ExoML projection check rejected valid input");
  CHECK(on1x_sdk_cdecl_parameter_count(&fixture.function, &count) ==
                ON1X_SDK_OK &&
            count == 2u,
        "parameter count accessor failed");
  CHECK(on1x_sdk_cdecl_parameter_at(&fixture.function, 1u, &parameter) ==
                ON1X_SDK_OK &&
            parameter == &fixture.parameters[1],
        "parameter accessor failed");
  CHECK(on1x_sdk_cdecl_parameter_at(&fixture.function, 2u, &parameter) ==
                ON1X_SDK_INVALID_ARGUMENT &&
            parameter == NULL,
        "out-of-range parameter accessor did not clear output");
  CHECK(on1x_sdk_exotract_switch_at(&fixture.exotract, 0u,
                                    &implementation_switch) == ON1X_SDK_OK &&
            implementation_switch.size == fixture.switches[0].size,
        "Exotract switch accessor failed");
  CHECK(on1x_sdk_exolib_profile_check(&fixture.exotract) == ON1X_SDK_OK &&
            on1x_sdk_kernel_discovery_profile_check(&fixture.exotract) ==
                ON1X_SDK_OK,
        "ExoLib/discovery projection adapters failed");
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_OK,
        "valid scalar admission rejected");
  CHECK(admission.admitted && admission.code == ON1X_SDK_ADMISSION_ADMITTED &&
            admission.machine_code.size && fixture.callbacks.reviews == 1u &&
            fixture.callbacks.boundaries == 1u &&
            fixture.callbacks.diagnostics == 0u,
        "scalar admission outputs/callback counts mismatch");
  return 0;
}

static int test_rejections(void) {
  ScalarFixture fixture;
  On1xSDK_ForeignAdmissionResult admission;
  initialize_fixture(&fixture);
  fixture.contract.effects_known = 0u;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_UNSUPPORTED &&
            admission.code == ON1X_SDK_ADMISSION_UNKNOWN_EFFECTS &&
            fixture.callbacks.diagnostics == 1u,
        "unknown effects did not fail closed");

  initialize_fixture(&fixture);
  fixture.parameter_claims[0].ownership = ON1X_SDK_OWNERSHIP_UNKNOWN;
  admission = result();
  CHECK(on1x_sdk_contract_claim_check(&fixture.function, &fixture.contract) ==
            ON1X_SDK_OK,
        "unknown ownership should remain a structurally valid claim");
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_UNSUPPORTED &&
            admission.code == ON1X_SDK_ADMISSION_UNKNOWN_OWNERSHIP,
        "unknown ownership did not fail closed");

  initialize_fixture(&fixture);
  fixture.function.result.flags = 0u;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_INVALID_ARGUMENT &&
            admission.code == ON1X_SDK_ADMISSION_UNKNOWN_LAYOUT,
        "unknown layout did not reject admission");

  initialize_fixture(&fixture);
  fixture.policy.target = TEXT("other-target");
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_ABI_MISMATCH &&
            admission.code == ON1X_SDK_ADMISSION_TARGET_MISMATCH,
        "target mismatch was not distinguished");

  initialize_fixture(&fixture);
  fixture.function.calling_convention = 77u;
  fixture.policy.calling_convention = 77u;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_INVALID_ARGUMENT &&
            admission.code == ON1X_SDK_ADMISSION_INVALID_DESCRIPTOR,
        "unknown policy-matched calling convention was admitted");

  initialize_fixture(&fixture);
  fixture.contract.source_revision = TEXT("revision-2");
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_INVALID_ARGUMENT &&
            admission.code == ON1X_SDK_ADMISSION_SOURCE_DRIFT,
        "source revision drift was not distinguished");

  initialize_fixture(&fixture);
  fixture.evidence.subject_sha256 =
      TEXT("9999999999999999999999999999999999999999999999999999999999999999");
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_INVALID_ARGUMENT &&
            admission.code == ON1X_SDK_ADMISSION_CONTRACT_DRIFT,
        "review subject drift was accepted");

  initialize_fixture(&fixture);
  fixture.policy.authorize_review = NULL;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_UNSUPPORTED &&
            admission.code == ON1X_SDK_ADMISSION_REVIEW_REQUIRED,
        "missing host review authority was accepted");

  initialize_fixture(&fixture);
  fixture.policy.struct_size =
      (uint32_t)(offsetof(On1xSDK_ForeignAdmissionPolicy, allowed_effects) +
                 sizeof(fixture.policy.allowed_effects));
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_UNSUPPORTED &&
            admission.code == ON1X_SDK_ADMISSION_REVIEW_REQUIRED,
        "short compatible policy prefix was read past its size");

  initialize_fixture(&fixture);
  fixture.callbacks.boundary_status = (On1xSDK_Status)99u;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_ERROR &&
            admission.code == ON1X_SDK_ADMISSION_CALLBACK_FAILED,
        "unknown host callback status was not normalized");

  initialize_fixture(&fixture);
  fixture.contract.effects_known = 0u;
  fixture.callbacks.diagnostic_status = ON1X_SDK_BUSY;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_BUSY &&
            admission.code == ON1X_SDK_ADMISSION_DIAGNOSTIC_FAILED,
        "diagnostic sink failure was not observable");

  initialize_fixture(&fixture);
  fixture.callbacks.reenter = 1;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_OK &&
            fixture.callbacks.nested_status == ON1X_SDK_BUSY,
        "recursive admission was not rejected");

  initialize_fixture(&fixture);
  fixture.candidate.abi_version = 99u;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_ABI_MISMATCH &&
            admission.code == ON1X_SDK_ADMISSION_ABI_VERSION,
        "foreign ABI mismatch was not distinguished");

  initialize_fixture(&fixture);
  fixture.function.result.abi_version = 99u;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_ABI_MISMATCH &&
            admission.code == ON1X_SDK_ADMISSION_ABI_VERSION,
        "nested foreign type ABI mismatch was not distinguished");

  initialize_fixture(&fixture);
  fixture.evidence.abi_version = 99u;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_ABI_MISMATCH &&
            admission.code == ON1X_SDK_ADMISSION_ABI_VERSION,
        "evidence ABI mismatch was not distinguished");
  return 0;
}

static int test_string_and_resource_profiles(void) {
  ScalarFixture fixture;
  On1xSDK_ForeignAdmissionResult admission;
  initialize_fixture(&fixture);
  fixture.function.c_name = TEXT("string_size");
  fixture.parameters[0].name = TEXT("text");
  fixture.parameters[0].type.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture.parameters[0].type.struct_size = sizeof(On1xSDK_ForeignType);
  fixture.parameters[0].type.kind = ON1X_SDK_FOREIGN_C_STRING;
  fixture.parameters[0].type.flags = ON1X_SDK_FOREIGN_TYPE_POINTER |
                                     ON1X_SDK_FOREIGN_TYPE_CONST_POINTEE |
                                     ON1X_SDK_FOREIGN_TYPE_NUL_TERMINATED;
  fixture.parameters[0].type.canonical_id = TEXT("const-char-ptr");
  fixture.parameters[0].type.nominal_id = (On1xSDK_FFIText){NULL, 0u};
  fixture.parameters[0].type.size_bytes = 8u;
  fixture.parameters[0].type.alignment_bytes = 8u;
  fixture.parameters[0].type.bit_width = 64u;
  fixture.function.parameter_count = 1u;
  fixture.contract.function_name = fixture.function.c_name;
  fixture.contract.parameter_count = 1u;
  fixture.parameter_claims[0].ownership = ON1X_SDK_OWNERSHIP_BORROWED_IMMUTABLE;
  fixture.parameter_claims[0].lifetime = ON1X_SDK_LIFETIME_CALL;
  fixture.contract.effects = ON1X_SDK_FOREIGN_EFFECT_READ;
  fixture.candidate.profile = ON1X_SDK_FOREIGN_PROFILE_IMMUTABLE_C_STRING;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_OK,
        "reviewed immutable C string profile rejected");

  initialize_fixture(&fixture);
  fixture.function.c_name = TEXT("thing_create");
  fixture.function.result.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture.function.result.struct_size = sizeof(On1xSDK_ForeignType);
  fixture.function.result.kind = ON1X_SDK_FOREIGN_OPAQUE_HANDLE;
  fixture.function.result.flags = ON1X_SDK_FOREIGN_TYPE_POINTER;
  fixture.function.result.canonical_id = TEXT("thing-ptr");
  fixture.function.result.nominal_id = TEXT("thing");
  fixture.function.result.size_bytes = 8u;
  fixture.function.result.alignment_bytes = 8u;
  fixture.function.result.bit_width = 64u;
  fixture.function.parameters = NULL;
  fixture.function.parameter_count = 0u;
  fixture.contract.function_name = fixture.function.c_name;
  fixture.contract.parameters = NULL;
  fixture.contract.parameter_count = 0u;
  fixture.contract.result.ownership = ON1X_SDK_OWNERSHIP_OWNED_UNIQUE;
  fixture.contract.result.nullable = 1u;
  fixture.contract.result.resource_id = TEXT("thing");
  fixture.contract.result.destroy_with = TEXT("thing_destroy");
  fixture.contract.effects = ON1X_SDK_FOREIGN_EFFECT_ALLOCATE;
  fixture.contract.error_kind = ON1X_SDK_FOREIGN_ERROR_NULL_ON_FAILURE;
  fixture.candidate.profile = ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_CREATE;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_OK,
        "reviewed opaque constructor profile rejected");
  fixture.contract.result.resource_id = TEXT("other-thing");
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_UNSUPPORTED &&
            admission.code == ON1X_SDK_ADMISSION_UNKNOWN_OWNERSHIP,
        "opaque constructor resource identity drift was admitted");
  fixture.contract.result.resource_id = TEXT("thing");
  fixture.contract.effects = 0u;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_UNSUPPORTED &&
            admission.code == ON1X_SDK_ADMISSION_UNSUPPORTED_EFFECT,
        "opaque constructor without allocation claim accepted");

  initialize_fixture(&fixture);
  fixture.function.c_name = TEXT("thing_read");
  fixture.parameters[0].type.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture.parameters[0].type.struct_size = sizeof(On1xSDK_ForeignType);
  fixture.parameters[0].type.kind = ON1X_SDK_FOREIGN_OPAQUE_HANDLE;
  fixture.parameters[0].type.flags = ON1X_SDK_FOREIGN_TYPE_POINTER;
  fixture.parameters[0].type.canonical_id = TEXT("thing-ptr");
  fixture.parameters[0].type.nominal_id = TEXT("thing");
  fixture.parameters[0].type.size_bytes = 8u;
  fixture.parameters[0].type.alignment_bytes = 8u;
  fixture.parameters[0].type.bit_width = 64u;
  fixture.function.parameter_count = 1u;
  fixture.contract.function_name = fixture.function.c_name;
  fixture.contract.parameter_count = 1u;
  fixture.parameter_claims[0].ownership = ON1X_SDK_OWNERSHIP_BORROWED_IMMUTABLE;
  fixture.parameter_claims[0].lifetime = ON1X_SDK_LIFETIME_CALL;
  fixture.contract.effects = ON1X_SDK_FOREIGN_EFFECT_READ;
  fixture.candidate.profile = ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_BORROW;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_OK,
        "reviewed opaque borrow profile rejected");

  initialize_fixture(&fixture);
  fixture.function.c_name = TEXT("thing_destroy");
  fixture.function.result = void_type();
  fixture.parameters[0].type.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  fixture.parameters[0].type.struct_size = sizeof(On1xSDK_ForeignType);
  fixture.parameters[0].type.kind = ON1X_SDK_FOREIGN_OPAQUE_HANDLE;
  fixture.parameters[0].type.flags = ON1X_SDK_FOREIGN_TYPE_POINTER;
  fixture.parameters[0].type.canonical_id = TEXT("thing-ptr");
  fixture.parameters[0].type.nominal_id = TEXT("thing");
  fixture.parameters[0].type.size_bytes = 8u;
  fixture.parameters[0].type.alignment_bytes = 8u;
  fixture.parameters[0].type.bit_width = 64u;
  fixture.function.parameter_count = 1u;
  fixture.contract.function_name = fixture.function.c_name;
  fixture.contract.parameter_count = 1u;
  fixture.parameter_claims[0].ownership = ON1X_SDK_OWNERSHIP_CONSUMED;
  fixture.parameter_claims[0].lifetime = ON1X_SDK_LIFETIME_NONE;
  fixture.contract.result.ownership = ON1X_SDK_OWNERSHIP_SCALAR;
  fixture.contract.result.lifetime = ON1X_SDK_LIFETIME_NONE;
  fixture.contract.effects = ON1X_SDK_FOREIGN_EFFECT_FREE;
  fixture.contract.error_kind = ON1X_SDK_FOREIGN_ERROR_NONE;
  fixture.candidate.profile = ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_DESTROY;
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_OK,
        "reviewed opaque destructor profile rejected");
  return 0;
}

static int test_malformed_and_legacy(void) {
  ScalarFixture fixture;
  On1xSDK_ForeignAdmissionResult admission = result();
  const char nonnull_empty[] = "";
  const unsigned char invalid_utf8[] = {0xc0u, 0x80u};
  On1xSDK_Contract legacy;
  initialize_fixture(&fixture);
  CHECK(on1x_sdk_ffi_text_valid((On1xSDK_FFIText){nonnull_empty, 0u}, 1),
        "non-null empty view rejected");
  CHECK(!on1x_sdk_ffi_text_valid(
            (On1xSDK_FFIText){(const char *)invalid_utf8, sizeof(invalid_utf8)},
            0),
        "malformed UTF-8 accepted");
  CHECK(!on1x_sdk_ffi_span_valid(
            &fixture.function.declaration_span,
            (On1xSDK_FFIText){NULL, fixture.source.revision.size},
            fixture.source.source_size),
        "invalid expected source revision was dereferenced or accepted");
  fixture.exotract.implementation_switch_count = 2u;
  {
    On1xSDK_FFIText duplicate_switches[2] = {TEXT("MULIB_IMPL_FIXTURE"),
                                             TEXT("MULIB_IMPL_FIXTURE")};
    fixture.exotract.implementation_switches = duplicate_switches;
    CHECK(on1x_sdk_exotract_profile_check(&fixture.exotract) ==
              ON1X_SDK_DUPLICATE,
          "duplicate implementation switch accepted");
  }
  memset(&legacy, 0, sizeof(legacy));
  legacy.abi_version = ON1X_SDK_ABI_VERSION;
  legacy.struct_size = sizeof(legacy);
  legacy.name = "legacy";
  legacy.signature = "i64 -> i64";
  CHECK(on1x_sdk_foreign_admit_v1_contract(&legacy, &admission) ==
                ON1X_SDK_UNSUPPORTED &&
            admission.code == ON1X_SDK_ADMISSION_LEGACY_METADATA_ONLY,
        "legacy metadata was treated as structured admission");
  CHECK(on1x_sdk_foreign_callback_supported() == ON1X_SDK_UNSUPPORTED,
        "general foreign callbacks were reported supported");
  CHECK(on1x_sdk_foreign_marshalling_supported() == ON1X_SDK_UNSUPPORTED &&
            on1x_sdk_foreign_module_loading_supported() ==
                ON1X_SDK_UNSUPPORTED &&
            on1x_sdk_binding_generator_api_supported() == ON1X_SDK_UNSUPPORTED,
        "unimplemented foreign activation APIs reported support");
  {
    struct CandidatePrefix {
      uint32_t abi_version;
      uint32_t struct_size;
    } short_candidate = {ON1X_SDK_FFI_ABI_VERSION, sizeof(short_candidate)};
    admission = result();
    CHECK(on1x_sdk_foreign_admit(
              (const On1xSDK_ForeignCandidate *)(const void *)&short_candidate,
              &fixture.policy, &admission) == ON1X_SDK_INVALID_ARGUMENT &&
              admission.code == ON1X_SDK_ADMISSION_INVALID_DESCRIPTOR,
          "short candidate prefix was read past its advertised size");
  }
  {
    struct FunctionPrefix {
      uint32_t abi_version;
      uint32_t struct_size;
    } short_function = {ON1X_SDK_FFI_ABI_VERSION, sizeof(short_function)};
    initialize_fixture(&fixture);
    fixture.candidate.declaration =
        (const On1xSDK_CDeclFunction *)(const void *)&short_function;
    admission = result();
    CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                                 &admission) == ON1X_SDK_INVALID_ARGUMENT &&
              admission.code == ON1X_SDK_ADMISSION_UNKNOWN_LAYOUT &&
              fixture.callbacks.diagnostics == 1u,
          "malformed CDecl diagnostic read past a short prefix");
  }
  initialize_fixture(&fixture);
  fixture.parameter_claims[0].owner_parameter = TEXT("left");
  admission = result();
  CHECK(on1x_sdk_foreign_admit(&fixture.candidate, &fixture.policy,
                               &admission) == ON1X_SDK_UNSUPPORTED &&
            admission.code == ON1X_SDK_ADMISSION_UNKNOWN_OWNERSHIP,
        "extraneous owner relationship was admitted");
  return 0;
}

int main(void) {
  int status;
  if ((status = test_scalar_and_accessors()))
    return status;
  if ((status = test_rejections()))
    return status;
  if ((status = test_string_and_resource_profiles()))
    return status;
  if ((status = test_malformed_and_legacy()))
    return status;
  return 0;
}
