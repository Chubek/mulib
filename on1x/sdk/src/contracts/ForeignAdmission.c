#include "On1xSDK-BindingGenerator.h"
#include "On1xSDK-ExoLib.h"
#include "On1xSDK-ExoML.h"
#include "On1xSDK-ForeignCallback.h"
#include "On1xSDK-ForeignModule.h"
#include "On1xSDK-KernelBind.h"
#include "On1xSDK-KernelDiscovery.h"
#include "On1xSDK-Marshalling.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define ON1X_FFI_REQUIRED(T, field)                                            \
  ((uint32_t)(offsetof(T, field) + sizeof(((T *)0)->field)))
#define ON1X_FFI_HAS(value, T, field)                                          \
  ((value) != NULL && (value)->struct_size >= ON1X_FFI_REQUIRED(T, field))

static _Thread_local int on1x_ffi_admission_active_;

static int on1x_ffi_prefix_(const void *value, uint32_t size,
                            uint32_t required) {
  const uint32_t *prefix = (const uint32_t *)value;
  return value && size >= 2u * sizeof(uint32_t) &&
         prefix[0] == ON1X_SDK_FFI_ABI_VERSION && prefix[1] >= required;
}

static int on1x_ffi_power_of_two_(uint32_t value) {
  return value && !(value & (value - 1u));
}

static int on1x_ffi_text_equal_(On1xSDK_FFIText left, On1xSDK_FFIText right) {
  return left.size == right.size &&
         (!left.size || !memcmp(left.data, right.data, left.size));
}

static int on1x_ffi_utf8_(const unsigned char *data, size_t size) {
  size_t i = 0;
  while (i < size) {
    uint32_t cp;
    unsigned need;
    const unsigned char first = data[i++];
    if (first < 0x80u)
      continue;
    if (first >= 0xc2u && first <= 0xdfu) {
      cp = (uint32_t)(first & 0x1fu);
      need = 1;
    } else if (first >= 0xe0u && first <= 0xefu) {
      cp = (uint32_t)(first & 0x0fu);
      need = 2;
    } else if (first >= 0xf0u && first <= 0xf4u) {
      cp = (uint32_t)(first & 0x07u);
      need = 3;
    } else {
      return 0;
    }
    if (need > size - i)
      return 0;
    for (unsigned j = 0; j < need; ++j) {
      const unsigned char byte = data[i++];
      if ((byte & 0xc0u) != 0x80u)
        return 0;
      cp = (cp << 6) | (uint32_t)(byte & 0x3fu);
    }
    if ((need == 2 && cp < 0x800u) || (need == 3 && cp < 0x10000u) ||
        cp > 0x10ffffu || (cp >= 0xd800u && cp <= 0xdfffu)) {
      return 0;
    }
  }
  return 1;
}

int on1x_sdk_ffi_text_valid(On1xSDK_FFIText text, int allow_empty) {
  if (text.size > ON1X_SDK_FFI_MAX_TEXT_BYTES)
    return 0;
  if (!text.size)
    return allow_empty;
  if (!text.data || memchr(text.data, 0, text.size))
    return 0;
  return on1x_ffi_utf8_((const unsigned char *)text.data, text.size);
}

int on1x_sdk_ffi_sha256_valid(On1xSDK_FFIText text) {
  if (!text.data || text.size != ON1X_SDK_FFI_SHA256_HEX_BYTES)
    return 0;
  for (size_t i = 0; i < text.size; ++i) {
    const unsigned char c = (unsigned char)text.data[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
      return 0;
  }
  return 1;
}

int on1x_sdk_ffi_span_valid(const On1xSDK_FFISpan *span,
                            On1xSDK_FFIText expected_revision,
                            uint64_t expected_source_size) {
  if (!span ||
      !on1x_ffi_prefix_(span, span->struct_size,
                        ON1X_FFI_REQUIRED(On1xSDK_FFISpan, source_size)) ||
      !on1x_sdk_ffi_text_valid(expected_revision, 0) || !expected_source_size ||
      !on1x_sdk_ffi_text_valid(span->revision, 0) ||
      !on1x_ffi_text_equal_(span->revision, expected_revision) ||
      span->source_size != expected_source_size || span->begin > span->end ||
      span->end > span->source_size) {
    return 0;
  }
  return 1;
}

static int on1x_ffi_identifier_(On1xSDK_FFIText text) {
  if (!on1x_sdk_ffi_text_valid(text, 0) || text.size > 256u)
    return 0;
  for (size_t i = 0; i < text.size; ++i) {
    const unsigned char c = (unsigned char)text.data[i];
    const int okay = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '_';
    if (!okay || (i == 0 && c >= '0' && c <= '9'))
      return 0;
  }
  return 1;
}

static int on1x_ffi_switch_(On1xSDK_FFIText text) {
  static const char prefix[] = "MULIB_IMPL_";
  return text.size > sizeof(prefix) - 1u && on1x_ffi_identifier_(text) &&
         !memcmp(text.data, prefix, sizeof(prefix) - 1u);
}

On1xSDK_Status on1x_sdk_foreign_type_check(const On1xSDK_ForeignType *type,
                                           uint32_t pointer_bits) {
  const uint32_t pointer_bytes = pointer_bits / 8u;
  const uint32_t flag_mask =
      ON1X_SDK_FOREIGN_TYPE_POINTER | ON1X_SDK_FOREIGN_TYPE_CONST_POINTEE |
      ON1X_SDK_FOREIGN_TYPE_VOLATILE | ON1X_SDK_FOREIGN_TYPE_RESTRICT |
      ON1X_SDK_FOREIGN_TYPE_IEEE754 | ON1X_SDK_FOREIGN_TYPE_NUL_TERMINATED |
      ON1X_SDK_FOREIGN_TYPE_COMPLETE_LAYOUT;
  if (!type)
    return ON1X_SDK_INVALID_ARGUMENT;
  if (type->abi_version != ON1X_SDK_FFI_ABI_VERSION)
    return ON1X_SDK_ABI_MISMATCH;
  if (!on1x_ffi_prefix_(type, type->struct_size,
                        ON1X_FFI_REQUIRED(On1xSDK_ForeignType, reserved)) ||
      (pointer_bits != 32u && pointer_bits != 64u) ||
      !on1x_sdk_ffi_text_valid(type->canonical_id, 0) || type->reserved) {
    return ON1X_SDK_INVALID_ARGUMENT;
  }
  if (type->flags & ~flag_mask) {
    return ON1X_SDK_INVALID_ARGUMENT;
  }
  switch (type->kind) {
  case ON1X_SDK_FOREIGN_VOID:
    return type->size_bytes || type->alignment_bytes || type->bit_width ||
                   type->flags || type->nominal_id.size
               ? ON1X_SDK_INVALID_ARGUMENT
               : ON1X_SDK_OK;
  case ON1X_SDK_FOREIGN_BOOL:
    return type->size_bytes == 1u && type->alignment_bytes == 1u &&
                   type->bit_width == 8u &&
                   type->flags == ON1X_SDK_FOREIGN_TYPE_COMPLETE_LAYOUT &&
                   !type->nominal_id.size
               ? ON1X_SDK_OK
               : ON1X_SDK_INVALID_ARGUMENT;
  case ON1X_SDK_FOREIGN_SIGNED_INTEGER:
  case ON1X_SDK_FOREIGN_UNSIGNED_INTEGER:
    if ((type->bit_width != 8u && type->bit_width != 16u &&
         type->bit_width != 32u && type->bit_width != 64u) ||
        type->size_bytes != type->bit_width / 8u ||
        !on1x_ffi_power_of_two_(type->alignment_bytes) ||
        type->alignment_bytes > type->size_bytes ||
        type->flags != ON1X_SDK_FOREIGN_TYPE_COMPLETE_LAYOUT ||
        type->nominal_id.size) {
      return ON1X_SDK_INVALID_ARGUMENT;
    }
    return ON1X_SDK_OK;
  case ON1X_SDK_FOREIGN_BINARY32:
  case ON1X_SDK_FOREIGN_BINARY64: {
    const uint32_t bits = type->kind == ON1X_SDK_FOREIGN_BINARY32 ? 32u : 64u;
    return type->bit_width == bits && type->size_bytes == bits / 8u &&
                   on1x_ffi_power_of_two_(type->alignment_bytes) &&
                   type->alignment_bytes <= type->size_bytes &&
                   type->flags == (ON1X_SDK_FOREIGN_TYPE_IEEE754 |
                                   ON1X_SDK_FOREIGN_TYPE_COMPLETE_LAYOUT) &&
                   !type->nominal_id.size
               ? ON1X_SDK_OK
               : ON1X_SDK_INVALID_ARGUMENT;
  }
  case ON1X_SDK_FOREIGN_C_STRING: {
    const uint32_t expected = ON1X_SDK_FOREIGN_TYPE_POINTER |
                              ON1X_SDK_FOREIGN_TYPE_CONST_POINTEE |
                              ON1X_SDK_FOREIGN_TYPE_NUL_TERMINATED;
    return type->size_bytes == pointer_bytes &&
                   type->bit_width == pointer_bits &&
                   on1x_ffi_power_of_two_(type->alignment_bytes) &&
                   type->alignment_bytes <= pointer_bytes &&
                   type->flags == expected && !type->nominal_id.size
               ? ON1X_SDK_OK
               : ON1X_SDK_INVALID_ARGUMENT;
  }
  case ON1X_SDK_FOREIGN_OPAQUE_HANDLE:
    return type->size_bytes == pointer_bytes &&
                   type->bit_width == pointer_bits &&
                   on1x_ffi_power_of_two_(type->alignment_bytes) &&
                   type->alignment_bytes <= pointer_bytes &&
                   type->flags == ON1X_SDK_FOREIGN_TYPE_POINTER &&
                   on1x_sdk_ffi_text_valid(type->nominal_id, 0)
               ? ON1X_SDK_OK
               : ON1X_SDK_INVALID_ARGUMENT;
  case ON1X_SDK_FOREIGN_UNSUPPORTED:
    return ON1X_SDK_UNSUPPORTED;
  }
  return ON1X_SDK_INVALID_ARGUMENT;
}

On1xSDK_Status on1x_sdk_cdecl_source_check(const On1xSDK_CDeclSource *source) {
  const uint32_t flag_mask =
      ON1X_SDK_CDECL_COMPLETE | ON1X_SDK_CDECL_HAS_DISCOVERY_ERRORS;
  if (!source)
    return ON1X_SDK_INVALID_ARGUMENT;
  if (source->abi_version != ON1X_SDK_FFI_ABI_VERSION)
    return ON1X_SDK_ABI_MISMATCH;
  if (!on1x_ffi_prefix_(source, source->struct_size,
                        ON1X_FFI_REQUIRED(On1xSDK_CDeclSource, reserved)) ||
      source->schema_version != ON1X_SDK_FFI_CDECL_SCHEMA_VERSION ||
      (source->pointer_bits != 32u && source->pointer_bits != 64u) ||
      !on1x_sdk_ffi_text_valid(source->header, 0) ||
      !on1x_sdk_ffi_text_valid(source->revision, 0) ||
      !on1x_sdk_ffi_sha256_valid(source->fingerprint_sha256) ||
      !on1x_sdk_ffi_sha256_valid(source->header_sha256) ||
      !on1x_sdk_ffi_text_valid(source->target, 0) ||
      !on1x_sdk_ffi_text_valid(source->compiler, 0) || !source->source_size ||
      (source->flags & ~flag_mask) || source->reserved) {
    return ON1X_SDK_INVALID_ARGUMENT;
  }
  if (!(source->flags & ON1X_SDK_CDECL_COMPLETE) ||
      (source->flags & ON1X_SDK_CDECL_HAS_DISCOVERY_ERRORS)) {
    return ON1X_SDK_UNSUPPORTED;
  }
  return ON1X_SDK_OK;
}

On1xSDK_Status
on1x_sdk_cdecl_function_check(const On1xSDK_CDeclFunction *function) {
  On1xSDK_Status status;
  const uint32_t flag_mask = ON1X_SDK_CDECL_PROTOTYPE | ON1X_SDK_CDECL_VARIADIC;
  if (!function)
    return ON1X_SDK_INVALID_ARGUMENT;
  if (function->abi_version != ON1X_SDK_FFI_ABI_VERSION)
    return ON1X_SDK_ABI_MISMATCH;
  if (!on1x_ffi_prefix_(
          function, function->struct_size,
          ON1X_FFI_REQUIRED(On1xSDK_CDeclFunction, declaration_span)) ||
      !function->source || !on1x_ffi_identifier_(function->c_name) ||
      function->parameter_count > ON1X_SDK_FFI_MAX_PARAMETERS ||
      (!function->parameters && function->parameter_count) ||
      function->calling_convention != ON1X_SDK_CDECL_CALL_C ||
      (function->flags & ~flag_mask)) {
    return ON1X_SDK_INVALID_ARGUMENT;
  }
  status = on1x_sdk_cdecl_source_check(function->source);
  if (status != ON1X_SDK_OK)
    return status;
  if (!(function->flags & ON1X_SDK_CDECL_PROTOTYPE) ||
      (function->flags & ON1X_SDK_CDECL_VARIADIC)) {
    return ON1X_SDK_UNSUPPORTED;
  }
  status = on1x_sdk_foreign_type_check(&function->result,
                                       function->source->pointer_bits);
  if (status != ON1X_SDK_OK)
    return status;
  if (!on1x_sdk_ffi_span_valid(&function->declaration_span,
                               function->source->revision,
                               function->source->source_size)) {
    return ON1X_SDK_INVALID_ARGUMENT;
  }
  for (size_t i = 0; i < function->parameter_count; ++i) {
    const On1xSDK_CDeclParameter *parameter = function->parameters + i;
    if (parameter->abi_version != ON1X_SDK_FFI_ABI_VERSION)
      return ON1X_SDK_ABI_MISMATCH;
    if (!on1x_ffi_prefix_(parameter, parameter->struct_size,
                          ON1X_FFI_REQUIRED(On1xSDK_CDeclParameter, source)) ||
        (!on1x_sdk_ffi_text_valid(parameter->name, 0) &&
         parameter->name.size) ||
        !on1x_sdk_ffi_span_valid(&parameter->source, function->source->revision,
                                 function->source->source_size)) {
      return ON1X_SDK_INVALID_ARGUMENT;
    }
    status = on1x_sdk_foreign_type_check(&parameter->type,
                                         function->source->pointer_bits);
    if (status != ON1X_SDK_OK ||
        parameter->type.kind == ON1X_SDK_FOREIGN_VOID) {
      return status == ON1X_SDK_OK ? ON1X_SDK_INVALID_ARGUMENT : status;
    }
  }
  return ON1X_SDK_OK;
}

On1xSDK_Status
on1x_sdk_cdecl_parameter_count(const On1xSDK_CDeclFunction *function,
                               size_t *count) {
  On1xSDK_Status status;
  if (!count)
    return ON1X_SDK_INVALID_ARGUMENT;
  *count = 0;
  status = on1x_sdk_cdecl_function_check(function);
  if (status != ON1X_SDK_OK)
    return status;
  *count = function->parameter_count;
  return ON1X_SDK_OK;
}

On1xSDK_Status
on1x_sdk_cdecl_parameter_at(const On1xSDK_CDeclFunction *function, size_t index,
                            const On1xSDK_CDeclParameter **parameter) {
  On1xSDK_Status status;
  if (!parameter)
    return ON1X_SDK_INVALID_ARGUMENT;
  *parameter = NULL;
  status = on1x_sdk_cdecl_function_check(function);
  if (status != ON1X_SDK_OK)
    return status;
  if (index >= function->parameter_count)
    return ON1X_SDK_INVALID_ARGUMENT;
  *parameter = function->parameters + index;
  return ON1X_SDK_OK;
}

static int on1x_ffi_ownership_(On1xSDK_ForeignOwnership ownership) {
  return ownership >= ON1X_SDK_OWNERSHIP_UNKNOWN &&
         ownership <= ON1X_SDK_OWNERSHIP_CONSUMED;
}

static int on1x_ffi_lifetime_(On1xSDK_ForeignLifetime lifetime) {
  return lifetime >= ON1X_SDK_LIFETIME_NONE &&
         lifetime <= ON1X_SDK_LIFETIME_OWNER;
}

On1xSDK_Status
on1x_sdk_contract_claim_check(const On1xSDK_CDeclFunction *function,
                              const On1xSDK_ForeignContractClaim *claim) {
  static const uint32_t effect_mask =
      ON1X_SDK_FOREIGN_EFFECT_READ | ON1X_SDK_FOREIGN_EFFECT_WRITE |
      ON1X_SDK_FOREIGN_EFFECT_ALLOCATE | ON1X_SDK_FOREIGN_EFFECT_FREE |
      ON1X_SDK_FOREIGN_EFFECT_ESCAPE | ON1X_SDK_FOREIGN_EFFECT_IO |
      ON1X_SDK_FOREIGN_EFFECT_THREAD | ON1X_SDK_FOREIGN_EFFECT_SUSPEND |
      ON1X_SDK_FOREIGN_EFFECT_FOREIGN_CALL | ON1X_SDK_FOREIGN_EFFECT_CALLBACK;
  On1xSDK_Status status = on1x_sdk_cdecl_function_check(function);
  uint64_t seen = 0;
  if (status != ON1X_SDK_OK)
    return status;
  if (!claim)
    return ON1X_SDK_INVALID_ARGUMENT;
  if (claim->abi_version != ON1X_SDK_FFI_ABI_VERSION)
    return ON1X_SDK_ABI_MISMATCH;
  if (!on1x_ffi_prefix_(
          claim, claim->struct_size,
          ON1X_FFI_REQUIRED(On1xSDK_ForeignContractClaim, may_reenter)) ||
      claim->schema_version != ON1X_SDK_FFI_CONTRACT_SCHEMA_VERSION ||
      claim->effects_known > 1u ||
      !on1x_ffi_identifier_(claim->function_name) ||
      !on1x_ffi_text_equal_(claim->function_name, function->c_name) ||
      !on1x_sdk_ffi_sha256_valid(claim->cdecl_fingerprint_sha256) ||
      !on1x_sdk_ffi_sha256_valid(claim->contract_fingerprint_sha256) ||
      !on1x_sdk_ffi_text_valid(claim->source_revision, 0) ||
      !on1x_ffi_text_equal_(claim->cdecl_fingerprint_sha256,
                            function->source->fingerprint_sha256) ||
      !on1x_ffi_text_equal_(claim->source_revision,
                            function->source->revision) ||
      claim->parameter_count != function->parameter_count ||
      (!claim->parameters && claim->parameter_count) ||
      (claim->effects & ~effect_mask) || claim->may_block > 1u ||
      claim->may_reenter > 1u ||
      (claim->thread_safety != ON1X_SDK_FOREIGN_SINGLE_THREAD &&
       claim->thread_safety != ON1X_SDK_FOREIGN_THREAD_SAFE &&
       claim->thread_safety != ON1X_SDK_FOREIGN_THREAD_AFFINE &&
       claim->thread_safety != ON1X_SDK_FOREIGN_THREAD_UNKNOWN) ||
      (claim->error_kind != ON1X_SDK_FOREIGN_ERROR_NONE &&
       claim->error_kind != ON1X_SDK_FOREIGN_ERROR_NULL_ON_FAILURE &&
       claim->error_kind != ON1X_SDK_FOREIGN_ERROR_BOOLEAN &&
       claim->error_kind != ON1X_SDK_FOREIGN_ERROR_INTEGER_CODE &&
       claim->error_kind != ON1X_SDK_FOREIGN_ERROR_UNSUPPORTED)) {
    return ON1X_SDK_INVALID_ARGUMENT;
  }
  for (size_t i = 0; i < claim->parameter_count; ++i) {
    const On1xSDK_ForeignParameterClaim *parameter = claim->parameters + i;
    if (parameter->abi_version != ON1X_SDK_FFI_ABI_VERSION)
      return ON1X_SDK_ABI_MISMATCH;
    if (!on1x_ffi_prefix_(parameter, parameter->struct_size,
                          ON1X_FFI_REQUIRED(On1xSDK_ForeignParameterClaim,
                                            owner_parameter)) ||
        parameter->index >= claim->parameter_count ||
        (seen & (UINT64_C(1) << parameter->index)) ||
        !on1x_ffi_ownership_(parameter->ownership) ||
        !on1x_ffi_lifetime_(parameter->lifetime) || parameter->nullable > 1u ||
        parameter->retained > 1u ||
        !on1x_sdk_ffi_text_valid(parameter->owner_parameter, 1)) {
      return ON1X_SDK_INVALID_ARGUMENT;
    }
    seen |= UINT64_C(1) << parameter->index;
  }
  if (claim->result.abi_version != ON1X_SDK_FFI_ABI_VERSION)
    return ON1X_SDK_ABI_MISMATCH;
  if (!on1x_ffi_prefix_(
          &claim->result, claim->result.struct_size,
          ON1X_FFI_REQUIRED(On1xSDK_ForeignReturnClaim, destroy_with)) ||
      !on1x_ffi_ownership_(claim->result.ownership) ||
      !on1x_ffi_lifetime_(claim->result.lifetime) ||
      claim->result.nullable > 1u || claim->result.reserved ||
      !on1x_sdk_ffi_text_valid(claim->result.resource_id, 1) ||
      !on1x_sdk_ffi_text_valid(claim->result.destroy_with, 1)) {
    return ON1X_SDK_INVALID_ARGUMENT;
  }
  return ON1X_SDK_OK;
}

On1xSDK_Status
on1x_sdk_exotract_profile_check(const On1xSDK_ExotractProfile *profile) {
  const uint32_t mask =
      ON1X_SDK_EXOTRACT_DISCOVERY_COMPLETE | ON1X_SDK_EXOTRACT_SYNTAX_CHECKED |
      ON1X_SDK_EXOTRACT_DECLARATION_ONLY | ON1X_SDK_EXOTRACT_DISCOVERY_ONLY;
  if (!profile)
    return ON1X_SDK_INVALID_ARGUMENT;
  if (profile->abi_version != ON1X_SDK_FFI_ABI_VERSION)
    return ON1X_SDK_ABI_MISMATCH;
  if (!on1x_ffi_prefix_(profile, profile->struct_size,
                        ON1X_FFI_REQUIRED(On1xSDK_ExotractProfile, reserved)) ||
      profile->schema_version != ON1X_SDK_EXOTRACT_SCHEMA_VERSION ||
      (profile->flags & ~mask) ||
      !on1x_sdk_ffi_text_valid(profile->library, 0) ||
      !on1x_sdk_ffi_sha256_valid(profile->manifest_sha256) ||
      !on1x_sdk_ffi_sha256_valid(profile->profile_sha256) ||
      profile->implementation_switch_count > ON1X_SDK_EXOTRACT_MAX_SWITCHES ||
      (!profile->implementation_switches &&
       profile->implementation_switch_count) ||
      profile->implementation_translation_units > 1u || profile->reserved) {
    return ON1X_SDK_INVALID_ARGUMENT;
  }
  if ((profile->flags & (ON1X_SDK_EXOTRACT_DISCOVERY_COMPLETE |
                         ON1X_SDK_EXOTRACT_SYNTAX_CHECKED |
                         ON1X_SDK_EXOTRACT_DECLARATION_ONLY)) !=
      (ON1X_SDK_EXOTRACT_DISCOVERY_COMPLETE | ON1X_SDK_EXOTRACT_SYNTAX_CHECKED |
       ON1X_SDK_EXOTRACT_DECLARATION_ONLY)) {
    return ON1X_SDK_UNSUPPORTED;
  }
  for (size_t i = 0; i < profile->implementation_switch_count; ++i) {
    if (!on1x_ffi_switch_(profile->implementation_switches[i]))
      return ON1X_SDK_INVALID_ARGUMENT;
    for (size_t j = 0; j < i; ++j) {
      if (on1x_ffi_text_equal_(profile->implementation_switches[i],
                               profile->implementation_switches[j]))
        return ON1X_SDK_DUPLICATE;
    }
  }
  return ON1X_SDK_OK;
}

On1xSDK_Status
on1x_sdk_exotract_switch_at(const On1xSDK_ExotractProfile *profile,
                            size_t index,
                            On1xSDK_FFIText *implementation_switch) {
  On1xSDK_Status status;
  if (!implementation_switch)
    return ON1X_SDK_INVALID_ARGUMENT;
  implementation_switch->data = NULL;
  implementation_switch->size = 0;
  status = on1x_sdk_exotract_profile_check(profile);
  if (status != ON1X_SDK_OK)
    return status;
  if (index >= profile->implementation_switch_count)
    return ON1X_SDK_INVALID_ARGUMENT;
  *implementation_switch = profile->implementation_switches[index];
  return ON1X_SDK_OK;
}

static int on1x_ffi_policy_(const On1xSDK_ForeignAdmissionPolicy *policy) {
  static const uint32_t effect_mask =
      ON1X_SDK_FOREIGN_EFFECT_READ | ON1X_SDK_FOREIGN_EFFECT_WRITE |
      ON1X_SDK_FOREIGN_EFFECT_ALLOCATE | ON1X_SDK_FOREIGN_EFFECT_FREE |
      ON1X_SDK_FOREIGN_EFFECT_ESCAPE | ON1X_SDK_FOREIGN_EFFECT_IO |
      ON1X_SDK_FOREIGN_EFFECT_THREAD | ON1X_SDK_FOREIGN_EFFECT_SUSPEND |
      ON1X_SDK_FOREIGN_EFFECT_FOREIGN_CALL | ON1X_SDK_FOREIGN_EFFECT_CALLBACK;
  return policy &&
         on1x_ffi_prefix_(policy, policy->struct_size,
                          ON1X_FFI_REQUIRED(On1xSDK_ForeignAdmissionPolicy,
                                            allowed_effects)) &&
         on1x_sdk_ffi_text_valid(policy->target, 0) &&
         (policy->pointer_bits == 32u || policy->pointer_bits == 64u) &&
         policy->max_parameters &&
         policy->max_parameters <= ON1X_SDK_FFI_MAX_PARAMETERS &&
         policy->calling_convention == ON1X_SDK_CDECL_CALL_C &&
         !(policy->allowed_effects & ~effect_mask);
}

static void *
on1x_ffi_callback_context_(const On1xSDK_ForeignAdmissionPolicy *policy) {
  return ON1X_FFI_HAS(policy, On1xSDK_ForeignAdmissionPolicy, callback_context)
             ? policy->callback_context
             : NULL;
}

static On1xSDK_ForeignReviewCallback
on1x_ffi_review_callback_(const On1xSDK_ForeignAdmissionPolicy *policy) {
  return ON1X_FFI_HAS(policy, On1xSDK_ForeignAdmissionPolicy, authorize_review)
             ? policy->authorize_review
             : NULL;
}

static On1xSDK_ForeignBoundaryCallback
on1x_ffi_boundary_callback_(const On1xSDK_ForeignAdmissionPolicy *policy) {
  return ON1X_FFI_HAS(policy, On1xSDK_ForeignAdmissionPolicy, verify_boundary)
             ? policy->verify_boundary
             : NULL;
}

static On1xSDK_FFIDiagnosticSink
on1x_ffi_diagnostic_sink_(const On1xSDK_ForeignAdmissionPolicy *policy) {
  return ON1X_FFI_HAS(policy, On1xSDK_ForeignAdmissionPolicy, diagnostic_sink)
             ? policy->diagnostic_sink
             : NULL;
}

static void *
on1x_ffi_diagnostic_context_(const On1xSDK_ForeignAdmissionPolicy *policy) {
  return ON1X_FFI_HAS(policy, On1xSDK_ForeignAdmissionPolicy,
                      diagnostic_context)
             ? policy->diagnostic_context
             : NULL;
}

On1xSDK_Status
on1x_sdk_contract_review_check(const On1xSDK_ForeignEvidence *evidence,
                               const On1xSDK_ForeignAdmissionPolicy *policy) {
  if (!evidence || !policy)
    return ON1X_SDK_INVALID_ARGUMENT;
  if (evidence->abi_version != ON1X_SDK_FFI_ABI_VERSION ||
      policy->abi_version != ON1X_SDK_FFI_ABI_VERSION)
    return ON1X_SDK_ABI_MISMATCH;
  if (!on1x_ffi_prefix_(
          evidence, evidence->struct_size,
          ON1X_FFI_REQUIRED(On1xSDK_ForeignEvidence, sanitizer_flags)) ||
      evidence->schema_version != ON1X_SDK_FFI_ADMISSION_SCHEMA_VERSION ||
      evidence->reserved ||
      !on1x_sdk_ffi_sha256_valid(evidence->subject_sha256) ||
      !on1x_sdk_ffi_text_valid(evidence->reviewer, 0) ||
      !on1x_sdk_ffi_text_valid(evidence->review_reference, 0) ||
      !on1x_sdk_ffi_sha256_valid(evidence->review_sha256) ||
      !on1x_sdk_ffi_sha256_valid(evidence->abi_probe_sha256) ||
      !on1x_sdk_ffi_sha256_valid(evidence->boundary_test_sha256) ||
      !on1x_sdk_ffi_text_valid(evidence->target, 0) ||
      !evidence->boundary_test_count || !on1x_ffi_policy_(policy)) {
    return ON1X_SDK_INVALID_ARGUMENT;
  }
  if (!on1x_ffi_text_equal_(evidence->target, policy->target))
    return ON1X_SDK_ABI_MISMATCH;
  return ON1X_SDK_OK;
}

static const char *on1x_ffi_code_name_(On1xSDK_ForeignAdmissionCode code) {
  switch (code) {
  case ON1X_SDK_ADMISSION_NONE:
    return "ON1X-FFI-NONE";
  case ON1X_SDK_ADMISSION_ADMITTED:
    return "ON1X-FFI-ADMITTED";
  case ON1X_SDK_ADMISSION_INVALID_DESCRIPTOR:
    return "ON1X-FFI-INVALID-DESCRIPTOR";
  case ON1X_SDK_ADMISSION_ABI_VERSION:
    return "ON1X-FFI-ABI-VERSION";
  case ON1X_SDK_ADMISSION_TARGET_MISMATCH:
    return "ON1X-FFI-TARGET-MISMATCH";
  case ON1X_SDK_ADMISSION_SOURCE_DRIFT:
    return "ON1X-FFI-SOURCE-DRIFT";
  case ON1X_SDK_ADMISSION_CONTRACT_DRIFT:
    return "ON1X-FFI-CONTRACT-DRIFT";
  case ON1X_SDK_ADMISSION_UNKNOWN_LAYOUT:
    return "ON1X-FFI-UNKNOWN-LAYOUT";
  case ON1X_SDK_ADMISSION_UNSUPPORTED_TYPE:
    return "ON1X-FFI-UNSUPPORTED-TYPE";
  case ON1X_SDK_ADMISSION_UNKNOWN_OWNERSHIP:
    return "ON1X-FFI-UNKNOWN-OWNERSHIP";
  case ON1X_SDK_ADMISSION_UNKNOWN_EFFECTS:
    return "ON1X-FFI-UNKNOWN-EFFECTS";
  case ON1X_SDK_ADMISSION_UNSUPPORTED_EFFECT:
    return "ON1X-FFI-UNSUPPORTED-EFFECT";
  case ON1X_SDK_ADMISSION_NULLABILITY:
    return "ON1X-FFI-NULLABILITY";
  case ON1X_SDK_ADMISSION_LIFETIME:
    return "ON1X-FFI-LIFETIME";
  case ON1X_SDK_ADMISSION_REVIEW_REQUIRED:
    return "ON1X-FFI-REVIEW-REQUIRED";
  case ON1X_SDK_ADMISSION_BOUNDARY_REQUIRED:
    return "ON1X-FFI-BOUNDARY-REQUIRED";
  case ON1X_SDK_ADMISSION_CALLBACK_FAILED:
    return "ON1X-FFI-CALLBACK-FAILED";
  case ON1X_SDK_ADMISSION_DIAGNOSTIC_FAILED:
    return "ON1X-FFI-DIAGNOSTIC-FAILED";
  case ON1X_SDK_ADMISSION_DISCOVERY_ONLY:
    return "ON1X-FFI-DISCOVERY-ONLY";
  case ON1X_SDK_ADMISSION_ONE_TU_REQUIRED:
    return "ON1X-FFI-ONE-TU-REQUIRED";
  case ON1X_SDK_ADMISSION_LEGACY_METADATA_ONLY:
    return "ON1X-FFI-LEGACY-METADATA-ONLY";
  }
  return "ON1X-FFI-UNKNOWN-CODE";
}

const char *
on1x_sdk_foreign_admission_code_name(On1xSDK_ForeignAdmissionCode code) {
  return on1x_ffi_code_name_(code);
}

static On1xSDK_FFIText on1x_ffi_static_text_(const char *text) {
  On1xSDK_FFIText result = {text, strlen(text)};
  return result;
}

static int on1x_ffi_result_prefix_(On1xSDK_ForeignAdmissionResult *result) {
  return result && on1x_ffi_prefix_(
                       result, result->struct_size,
                       ON1X_FFI_REQUIRED(On1xSDK_ForeignAdmissionResult, code));
}

static void on1x_ffi_result_(On1xSDK_ForeignAdmissionResult *result,
                             On1xSDK_ForeignAdmissionCode code, int admitted) {
  if (!on1x_ffi_result_prefix_(result))
    return;
  result->admitted = admitted ? 1u : 0u;
  result->code = code;
  if (result->struct_size >=
      ON1X_FFI_REQUIRED(On1xSDK_ForeignAdmissionResult, machine_code))
    result->machine_code = on1x_ffi_static_text_(on1x_ffi_code_name_(code));
}

static On1xSDK_Status on1x_ffi_callback_status_(On1xSDK_Status status) {
  const uint32_t value = (uint32_t)status;
  return value <= (uint32_t)ON1X_SDK_BUSY ? status : ON1X_SDK_ERROR;
}

static On1xSDK_Status
on1x_ffi_diagnostic_(const On1xSDK_ForeignAdmissionPolicy *policy,
                     const On1xSDK_CDeclFunction *function,
                     On1xSDK_ForeignAdmissionCode code, const char *message,
                     On1xSDK_ForeignAdmissionResult *result,
                     On1xSDK_Status operation_status) {
  On1xSDK_FFIDiagnostic diagnostic;
  On1xSDK_Status sink_status;
  const On1xSDK_FFIDiagnosticSink sink = on1x_ffi_diagnostic_sink_(policy);
  const int was_active = on1x_ffi_admission_active_;
  if (!sink)
    return operation_status;
  memset(&diagnostic, 0, sizeof(diagnostic));
  diagnostic.abi_version = ON1X_SDK_FFI_ABI_VERSION;
  diagnostic.struct_size = sizeof(diagnostic);
  diagnostic.code = on1x_ffi_static_text_(on1x_ffi_code_name_(code));
  diagnostic.message = on1x_ffi_static_text_(message);
  diagnostic.severity = ON1X_SDK_FFI_ERROR_SEVERITY;
  if (function &&
      on1x_ffi_prefix_(
          function, function->struct_size,
          ON1X_FFI_REQUIRED(On1xSDK_CDeclFunction, declaration_span)) &&
      function->source &&
      on1x_sdk_cdecl_source_check(function->source) == ON1X_SDK_OK &&
      on1x_sdk_ffi_span_valid(&function->declaration_span,
                              function->source->revision,
                              function->source->source_size)) {
    diagnostic.primary = function->declaration_span;
  } else {
    diagnostic.primary.abi_version = ON1X_SDK_FFI_ABI_VERSION;
    diagnostic.primary.struct_size = sizeof(diagnostic.primary);
  }
  on1x_ffi_admission_active_ = 1;
  sink_status = on1x_ffi_callback_status_(
      sink(on1x_ffi_diagnostic_context_(policy), &diagnostic));
  on1x_ffi_admission_active_ = was_active;
  if (sink_status != ON1X_SDK_OK) {
    on1x_ffi_result_(result, ON1X_SDK_ADMISSION_DIAGNOSTIC_FAILED, 0);
    return sink_status;
  }
  return operation_status;
}

static On1xSDK_Status
on1x_ffi_reject_(const On1xSDK_ForeignAdmissionPolicy *policy,
                 const On1xSDK_CDeclFunction *function,
                 On1xSDK_ForeignAdmissionResult *result,
                 On1xSDK_ForeignAdmissionCode code, On1xSDK_Status status,
                 const char *message) {
  on1x_ffi_result_(result, code, 0);
  return on1x_ffi_diagnostic_(policy, function, code, message, result, status);
}

static int on1x_ffi_scalar_kind_(On1xSDK_ForeignTypeKind kind, int allow_void) {
  return (allow_void && kind == ON1X_SDK_FOREIGN_VOID) ||
         kind == ON1X_SDK_FOREIGN_BOOL ||
         kind == ON1X_SDK_FOREIGN_SIGNED_INTEGER ||
         kind == ON1X_SDK_FOREIGN_UNSIGNED_INTEGER ||
         kind == ON1X_SDK_FOREIGN_BINARY32 || kind == ON1X_SDK_FOREIGN_BINARY64;
}

static const On1xSDK_ForeignParameterClaim *
on1x_ffi_claim_at_(const On1xSDK_ForeignContractClaim *claim, size_t index) {
  for (size_t i = 0; i < claim->parameter_count; ++i)
    if (claim->parameters[i].index == index)
      return claim->parameters + i;
  return NULL;
}

static int
on1x_ffi_plain_parameter_(const On1xSDK_ForeignParameterClaim *parameter) {
  return parameter && !parameter->retained && !parameter->owner_parameter.size;
}

static int on1x_ffi_plain_result_(const On1xSDK_ForeignReturnClaim *result) {
  return result && !result->resource_id.size && !result->destroy_with.size;
}

static On1xSDK_Status
on1x_ffi_profile_(const On1xSDK_ForeignCandidate *candidate,
                  On1xSDK_ForeignAdmissionCode *code) {
  const On1xSDK_CDeclFunction *function = candidate->declaration;
  const On1xSDK_ForeignContractClaim *claim = candidate->contract;
  int saw_string = function->result.kind == ON1X_SDK_FOREIGN_C_STRING;
  int saw_handle = function->result.kind == ON1X_SDK_FOREIGN_OPAQUE_HANDLE;
  for (size_t i = 0; i < function->parameter_count; ++i) {
    const On1xSDK_CDeclParameter *parameter = function->parameters + i;
    const On1xSDK_ForeignParameterClaim *p = on1x_ffi_claim_at_(claim, i);
    if (!p || p->ownership == ON1X_SDK_OWNERSHIP_UNKNOWN) {
      *code = ON1X_SDK_ADMISSION_UNKNOWN_OWNERSHIP;
      return ON1X_SDK_UNSUPPORTED;
    }
    if (parameter->type.kind == ON1X_SDK_FOREIGN_C_STRING)
      saw_string = 1;
    if (parameter->type.kind == ON1X_SDK_FOREIGN_OPAQUE_HANDLE)
      saw_handle = 1;
  }
  if (claim->result.ownership == ON1X_SDK_OWNERSHIP_UNKNOWN) {
    *code = ON1X_SDK_ADMISSION_UNKNOWN_OWNERSHIP;
    return ON1X_SDK_UNSUPPORTED;
  }
  switch (candidate->profile) {
  case ON1X_SDK_FOREIGN_PROFILE_SCALAR:
    if (!on1x_ffi_scalar_kind_(function->result.kind, 1) || saw_string ||
        saw_handle) {
      *code = ON1X_SDK_ADMISSION_UNSUPPORTED_TYPE;
      return ON1X_SDK_UNSUPPORTED;
    }
    if (claim->result.ownership != ON1X_SDK_OWNERSHIP_SCALAR ||
        claim->result.lifetime != ON1X_SDK_LIFETIME_NONE ||
        claim->result.nullable || !on1x_ffi_plain_result_(&claim->result) ||
        claim->error_kind != ON1X_SDK_FOREIGN_ERROR_NONE)
      goto ownership;
    for (size_t i = 0; i < function->parameter_count; ++i) {
      const On1xSDK_ForeignParameterClaim *p = on1x_ffi_claim_at_(claim, i);
      if (!on1x_ffi_scalar_kind_(function->parameters[i].type.kind, 0) ||
          p->ownership != ON1X_SDK_OWNERSHIP_SCALAR ||
          p->lifetime != ON1X_SDK_LIFETIME_NONE || p->nullable ||
          !on1x_ffi_plain_parameter_(p))
        goto ownership;
    }
    return ON1X_SDK_OK;
  case ON1X_SDK_FOREIGN_PROFILE_IMMUTABLE_C_STRING:
    if (!saw_string || saw_handle) {
      *code = ON1X_SDK_ADMISSION_UNSUPPORTED_TYPE;
      return ON1X_SDK_UNSUPPORTED;
    }
    for (size_t i = 0; i < function->parameter_count; ++i) {
      const On1xSDK_ForeignParameterClaim *p = on1x_ffi_claim_at_(claim, i);
      if (function->parameters[i].type.kind == ON1X_SDK_FOREIGN_C_STRING) {
        if (p->ownership != ON1X_SDK_OWNERSHIP_BORROWED_IMMUTABLE ||
            p->lifetime != ON1X_SDK_LIFETIME_CALL || p->nullable ||
            !on1x_ffi_plain_parameter_(p)) {
          *code = p->nullable ? ON1X_SDK_ADMISSION_NULLABILITY
                              : ON1X_SDK_ADMISSION_LIFETIME;
          return ON1X_SDK_UNSUPPORTED;
        }
      } else if (!on1x_ffi_scalar_kind_(function->parameters[i].type.kind, 0) ||
                 p->ownership != ON1X_SDK_OWNERSHIP_SCALAR ||
                 p->lifetime != ON1X_SDK_LIFETIME_NONE || p->nullable ||
                 !on1x_ffi_plain_parameter_(p)) {
        goto ownership;
      }
    }
    if (function->result.kind == ON1X_SDK_FOREIGN_C_STRING) {
      if (claim->result.ownership != ON1X_SDK_OWNERSHIP_BORROWED_IMMUTABLE ||
          claim->result.lifetime != ON1X_SDK_LIFETIME_GLOBAL_STATIC ||
          claim->result.nullable || !on1x_ffi_plain_result_(&claim->result)) {
        *code = claim->result.nullable ? ON1X_SDK_ADMISSION_NULLABILITY
                                       : ON1X_SDK_ADMISSION_LIFETIME;
        return ON1X_SDK_UNSUPPORTED;
      }
    } else if (!on1x_ffi_scalar_kind_(function->result.kind, 1) ||
               claim->result.ownership != ON1X_SDK_OWNERSHIP_SCALAR ||
               claim->result.lifetime != ON1X_SDK_LIFETIME_NONE ||
               claim->result.nullable ||
               !on1x_ffi_plain_result_(&claim->result)) {
      goto ownership;
    }
    if (claim->error_kind != ON1X_SDK_FOREIGN_ERROR_NONE)
      goto ownership;
    return ON1X_SDK_OK;
  case ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_CREATE:
    if (function->result.kind != ON1X_SDK_FOREIGN_OPAQUE_HANDLE ||
        !saw_handle) {
      *code = ON1X_SDK_ADMISSION_UNSUPPORTED_TYPE;
      return ON1X_SDK_UNSUPPORTED;
    }
    if (!claim->result.nullable) {
      *code = ON1X_SDK_ADMISSION_NULLABILITY;
      return ON1X_SDK_UNSUPPORTED;
    }
    if (claim->result.lifetime != ON1X_SDK_LIFETIME_NONE) {
      *code = ON1X_SDK_ADMISSION_LIFETIME;
      return ON1X_SDK_UNSUPPORTED;
    }
    if (claim->result.ownership != ON1X_SDK_OWNERSHIP_OWNED_UNIQUE ||
        !on1x_sdk_ffi_text_valid(claim->result.resource_id, 0) ||
        !on1x_ffi_text_equal_(claim->result.resource_id,
                              function->result.nominal_id) ||
        !on1x_ffi_identifier_(claim->result.destroy_with) ||
        claim->error_kind != ON1X_SDK_FOREIGN_ERROR_NULL_ON_FAILURE) {
      *code = ON1X_SDK_ADMISSION_UNKNOWN_OWNERSHIP;
      return ON1X_SDK_UNSUPPORTED;
    }
    for (size_t i = 0; i < function->parameter_count; ++i) {
      const On1xSDK_ForeignParameterClaim *p = on1x_ffi_claim_at_(claim, i);
      if (!on1x_ffi_scalar_kind_(function->parameters[i].type.kind, 0) ||
          p->ownership != ON1X_SDK_OWNERSHIP_SCALAR ||
          p->lifetime != ON1X_SDK_LIFETIME_NONE || p->nullable ||
          !on1x_ffi_plain_parameter_(p))
        goto ownership;
    }
    return ON1X_SDK_OK;
  case ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_BORROW:
    if (!saw_handle ||
        function->result.kind == ON1X_SDK_FOREIGN_OPAQUE_HANDLE) {
      *code = ON1X_SDK_ADMISSION_UNSUPPORTED_TYPE;
      return ON1X_SDK_UNSUPPORTED;
    }
    for (size_t i = 0; i < function->parameter_count; ++i) {
      const On1xSDK_ForeignParameterClaim *p = on1x_ffi_claim_at_(claim, i);
      if (function->parameters[i].type.kind == ON1X_SDK_FOREIGN_OPAQUE_HANDLE) {
        if (p->ownership != ON1X_SDK_OWNERSHIP_BORROWED_IMMUTABLE ||
            p->lifetime != ON1X_SDK_LIFETIME_CALL || p->nullable ||
            !on1x_ffi_plain_parameter_(p)) {
          *code = ON1X_SDK_ADMISSION_LIFETIME;
          return ON1X_SDK_UNSUPPORTED;
        }
      } else if (!on1x_ffi_scalar_kind_(function->parameters[i].type.kind, 0) ||
                 p->ownership != ON1X_SDK_OWNERSHIP_SCALAR ||
                 p->lifetime != ON1X_SDK_LIFETIME_NONE || p->nullable ||
                 !on1x_ffi_plain_parameter_(p)) {
        goto ownership;
      }
    }
    if (!on1x_ffi_scalar_kind_(function->result.kind, 1) ||
        claim->result.ownership != ON1X_SDK_OWNERSHIP_SCALAR ||
        claim->result.lifetime != ON1X_SDK_LIFETIME_NONE ||
        claim->result.nullable || !on1x_ffi_plain_result_(&claim->result) ||
        claim->error_kind != ON1X_SDK_FOREIGN_ERROR_NONE)
      goto ownership;
    return ON1X_SDK_OK;
  case ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_DESTROY:
    if (function->result.kind != ON1X_SDK_FOREIGN_VOID ||
        function->parameter_count != 1u ||
        function->parameters[0].type.kind != ON1X_SDK_FOREIGN_OPAQUE_HANDLE) {
      *code = ON1X_SDK_ADMISSION_UNSUPPORTED_TYPE;
      return ON1X_SDK_UNSUPPORTED;
    } else {
      const On1xSDK_ForeignParameterClaim *p = on1x_ffi_claim_at_(claim, 0);
      if (p->ownership != ON1X_SDK_OWNERSHIP_CONSUMED ||
          p->lifetime != ON1X_SDK_LIFETIME_NONE || p->nullable ||
          !on1x_ffi_plain_parameter_(p) ||
          claim->result.ownership != ON1X_SDK_OWNERSHIP_SCALAR ||
          claim->result.lifetime != ON1X_SDK_LIFETIME_NONE ||
          claim->result.nullable || !on1x_ffi_plain_result_(&claim->result) ||
          claim->error_kind != ON1X_SDK_FOREIGN_ERROR_NONE)
        goto ownership;
    }
    return ON1X_SDK_OK;
  }
  *code = ON1X_SDK_ADMISSION_INVALID_DESCRIPTOR;
  return ON1X_SDK_INVALID_ARGUMENT;

ownership:
  *code = ON1X_SDK_ADMISSION_UNKNOWN_OWNERSHIP;
  return ON1X_SDK_UNSUPPORTED;
}

On1xSDK_Status
on1x_sdk_foreign_admit(const On1xSDK_ForeignCandidate *candidate,
                       const On1xSDK_ForeignAdmissionPolicy *policy,
                       On1xSDK_ForeignAdmissionResult *result) {
  On1xSDK_Status status;
  On1xSDK_ForeignAdmissionCode code = ON1X_SDK_ADMISSION_INVALID_DESCRIPTOR;
  const On1xSDK_CDeclFunction *function = NULL;
  if (!on1x_ffi_result_prefix_(result))
    return ON1X_SDK_INVALID_ARGUMENT;
  on1x_ffi_result_(result, ON1X_SDK_ADMISSION_NONE, 0);
  if (on1x_ffi_admission_active_) {
    on1x_ffi_result_(result, ON1X_SDK_ADMISSION_CALLBACK_FAILED, 0);
    return ON1X_SDK_BUSY;
  }
  if ((candidate && candidate->abi_version != ON1X_SDK_FFI_ABI_VERSION) ||
      (policy && policy->abi_version != ON1X_SDK_FFI_ABI_VERSION)) {
    return on1x_ffi_reject_(
        NULL, NULL, result, ON1X_SDK_ADMISSION_ABI_VERSION,
        ON1X_SDK_ABI_MISMATCH,
        "foreign candidate or policy ABI version is unsupported");
  }
  if (!candidate ||
      !on1x_ffi_prefix_(
          candidate, candidate->struct_size,
          ON1X_FFI_REQUIRED(On1xSDK_ForeignCandidate, evidence))) {
    return on1x_ffi_reject_(policy, NULL, result,
                            ON1X_SDK_ADMISSION_INVALID_DESCRIPTOR,
                            ON1X_SDK_INVALID_ARGUMENT,
                            "malformed foreign candidate descriptor prefix");
  }
  function = candidate->declaration;
  if (candidate->admission_schema_version !=
          ON1X_SDK_FFI_ADMISSION_SCHEMA_VERSION ||
      !candidate->declaration || !candidate->contract || !candidate->exotract ||
      !candidate->evidence || !on1x_ffi_policy_(policy)) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_INVALID_DESCRIPTOR,
        ON1X_SDK_INVALID_ARGUMENT,
        "malformed foreign candidate, policy, or result descriptor");
  }
  status = on1x_sdk_cdecl_function_check(candidate->declaration);
  if (status != ON1X_SDK_OK) {
    code = status == ON1X_SDK_ABI_MISMATCH ? ON1X_SDK_ADMISSION_ABI_VERSION
           : status == ON1X_SDK_UNSUPPORTED
               ? ON1X_SDK_ADMISSION_UNSUPPORTED_TYPE
               : ON1X_SDK_ADMISSION_UNKNOWN_LAYOUT;
    return on1x_ffi_reject_(
        policy, function, result, code, status,
        "CDecl projection is incomplete, malformed, or unsupported");
  }
  if (!on1x_ffi_text_equal_(function->source->target, policy->target) ||
      function->source->pointer_bits != policy->pointer_bits ||
      function->calling_convention != policy->calling_convention) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_TARGET_MISMATCH,
        ON1X_SDK_ABI_MISMATCH,
        "target, pointer width, or calling convention does not match policy");
  }
  if (function->parameter_count > policy->max_parameters) {
    return on1x_ffi_reject_(policy, function, result,
                            ON1X_SDK_ADMISSION_UNSUPPORTED_TYPE, ON1X_SDK_LIMIT,
                            "foreign parameter limit exceeded");
  }
  status = on1x_sdk_contract_claim_check(function, candidate->contract);
  if (status != ON1X_SDK_OK) {
    code = status == ON1X_SDK_ABI_MISMATCH ? ON1X_SDK_ADMISSION_ABI_VERSION
                                           : ON1X_SDK_ADMISSION_CONTRACT_DRIFT;
    if (status == ON1X_SDK_INVALID_ARGUMENT &&
        on1x_ffi_prefix_(
            candidate->contract, candidate->contract->struct_size,
            ON1X_FFI_REQUIRED(On1xSDK_ForeignContractClaim, may_reenter)) &&
        (!on1x_ffi_text_equal_(candidate->contract->cdecl_fingerprint_sha256,
                               function->source->fingerprint_sha256) ||
         !on1x_ffi_text_equal_(candidate->contract->source_revision,
                               function->source->revision))) {
      code = ON1X_SDK_ADMISSION_SOURCE_DRIFT;
    }
    return on1x_ffi_reject_(
        policy, function, result, code, status,
        "contract claim does not match the exact CDecl source identity");
  }
  status = on1x_sdk_exotract_profile_check(candidate->exotract);
  if (status != ON1X_SDK_OK) {
    return on1x_ffi_reject_(
        policy, function, result,
        status == ON1X_SDK_ABI_MISMATCH ? ON1X_SDK_ADMISSION_ABI_VERSION
                                        : ON1X_SDK_ADMISSION_INVALID_DESCRIPTOR,
        status, "Exotract profile is malformed or incomplete");
  }
  if (candidate->exotract->flags & ON1X_SDK_EXOTRACT_DISCOVERY_ONLY) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_DISCOVERY_ONLY,
        ON1X_SDK_UNSUPPORTED, "discovery-only profiles cannot be admitted");
  }
  if (candidate->exotract->implementation_translation_units != 1u) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_ONE_TU_REQUIRED,
        ON1X_SDK_INVALID_ARGUMENT,
        "header-only implementation must use exactly one implementation TU");
  }
  if (!candidate->contract->effects_known) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_UNKNOWN_EFFECTS,
        ON1X_SDK_UNSUPPORTED, "unknown foreign effects reject safe admission");
  }
  if ((candidate->contract->effects & ~policy->allowed_effects) ||
      (candidate->contract->effects &
       (ON1X_SDK_FOREIGN_EFFECT_ESCAPE | ON1X_SDK_FOREIGN_EFFECT_THREAD |
        ON1X_SDK_FOREIGN_EFFECT_SUSPEND | ON1X_SDK_FOREIGN_EFFECT_CALLBACK)) ||
      candidate->contract->may_block || candidate->contract->may_reenter ||
      candidate->contract->thread_safety == ON1X_SDK_FOREIGN_THREAD_UNKNOWN ||
      candidate->contract->thread_safety == ON1X_SDK_FOREIGN_THREAD_AFFINE) {
    return on1x_ffi_reject_(policy, function, result,
                            ON1X_SDK_ADMISSION_UNSUPPORTED_EFFECT,
                            ON1X_SDK_UNSUPPORTED,
                            "foreign effects, blocking, reentrancy, or thread "
                            "affinity are outside this profile");
  }
  if ((candidate->contract->effects & ON1X_SDK_FOREIGN_EFFECT_ALLOCATE) &&
      candidate->profile != ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_CREATE) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_UNSUPPORTED_EFFECT,
        ON1X_SDK_UNSUPPORTED,
        "allocation requires the opaque-handle constructor profile");
  }
  if ((candidate->contract->effects & ON1X_SDK_FOREIGN_EFFECT_FREE) &&
      candidate->profile != ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_DESTROY) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_UNSUPPORTED_EFFECT,
        ON1X_SDK_UNSUPPORTED,
        "free requires the opaque-handle destructor profile");
  }
  if (candidate->profile == ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_CREATE &&
      !(candidate->contract->effects & ON1X_SDK_FOREIGN_EFFECT_ALLOCATE)) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_UNSUPPORTED_EFFECT,
        ON1X_SDK_UNSUPPORTED,
        "opaque-handle constructors must declare allocation");
  }
  if (candidate->profile == ON1X_SDK_FOREIGN_PROFILE_OPAQUE_HANDLE_DESTROY &&
      !(candidate->contract->effects & ON1X_SDK_FOREIGN_EFFECT_FREE)) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_UNSUPPORTED_EFFECT,
        ON1X_SDK_UNSUPPORTED, "opaque-handle destructors must declare free");
  }
  status = on1x_ffi_profile_(candidate, &code);
  if (status != ON1X_SDK_OK) {
    return on1x_ffi_reject_(policy, function, result, code, status,
                            "foreign type, ownership, nullability, or lifetime "
                            "is not admitted by the selected profile");
  }
  status = on1x_sdk_contract_review_check(candidate->evidence, policy);
  if (status != ON1X_SDK_OK) {
    return on1x_ffi_reject_(
        policy, function, result,
        status == ON1X_SDK_ABI_MISMATCH
            ? candidate->evidence->abi_version != ON1X_SDK_FFI_ABI_VERSION
                  ? ON1X_SDK_ADMISSION_ABI_VERSION
                  : ON1X_SDK_ADMISSION_TARGET_MISMATCH
            : ON1X_SDK_ADMISSION_REVIEW_REQUIRED,
        status, "review or executable evidence is missing or malformed");
  }
  if (!on1x_ffi_text_equal_(candidate->evidence->subject_sha256,
                            candidate->contract->contract_fingerprint_sha256)) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_CONTRACT_DRIFT,
        ON1X_SDK_INVALID_ARGUMENT,
        "review evidence does not identify the exact contract");
  }
  if (!on1x_ffi_review_callback_(policy)) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_REVIEW_REQUIRED,
        ON1X_SDK_UNSUPPORTED, "host review authorization callback is required");
  }
  if (!on1x_ffi_boundary_callback_(policy)) {
    return on1x_ffi_reject_(
        policy, function, result, ON1X_SDK_ADMISSION_BOUNDARY_REQUIRED,
        ON1X_SDK_UNSUPPORTED, "host executable-boundary callback is required");
  }
  on1x_ffi_admission_active_ = 1;
  status = on1x_ffi_callback_status_(on1x_ffi_review_callback_(policy)(
      on1x_ffi_callback_context_(policy), candidate, candidate->evidence));
  if (status != ON1X_SDK_OK) {
    on1x_ffi_admission_active_ = 0;
    return on1x_ffi_reject_(policy, function, result,
                            ON1X_SDK_ADMISSION_CALLBACK_FAILED, status,
                            "host review authorization callback failed");
  }
  status = on1x_ffi_callback_status_(on1x_ffi_boundary_callback_(policy)(
      on1x_ffi_callback_context_(policy), candidate, candidate->evidence));
  on1x_ffi_admission_active_ = 0;
  if (status != ON1X_SDK_OK) {
    return on1x_ffi_reject_(policy, function, result,
                            ON1X_SDK_ADMISSION_CALLBACK_FAILED, status,
                            "host executable-boundary callback failed");
  }
  on1x_ffi_result_(result, ON1X_SDK_ADMISSION_ADMITTED, 1);
  return ON1X_SDK_OK;
}

On1xSDK_Status
on1x_sdk_foreign_admit_v1_contract(const On1xSDK_Contract *legacy,
                                   On1xSDK_ForeignAdmissionResult *result) {
  if (!on1x_ffi_result_prefix_(result))
    return ON1X_SDK_INVALID_ARGUMENT;
  on1x_ffi_result_(result, ON1X_SDK_ADMISSION_LEGACY_METADATA_ONLY, 0);
  if (!legacy || legacy->abi_version != ON1X_SDK_ABI_VERSION ||
      legacy->struct_size < sizeof(On1xSDK_Contract) || !legacy->name ||
      !*legacy->name || !legacy->signature || !*legacy->signature) {
    on1x_ffi_result_(result, ON1X_SDK_ADMISSION_INVALID_DESCRIPTOR, 0);
    return ON1X_SDK_INVALID_ARGUMENT;
  }
  return ON1X_SDK_UNSUPPORTED;
}

On1xSDK_Status
on1x_sdk_exoml_projection_check(const On1xSDK_CDeclFunction *function,
                                const On1xSDK_ForeignContractClaim *claim) {
  return on1x_sdk_contract_claim_check(function, claim);
}

On1xSDK_Status on1x_sdk_foreign_callback_supported(void) {
  return ON1X_SDK_UNSUPPORTED;
}

On1xSDK_Status
on1x_sdk_exolib_profile_check(const On1xSDK_ExotractProfile *profile) {
  return on1x_sdk_exotract_profile_check(profile);
}

On1xSDK_Status on1x_sdk_kernel_discovery_profile_check(
    const On1xSDK_ExotractProfile *profile) {
  return on1x_sdk_exotract_profile_check(profile);
}

On1xSDK_Status on1x_sdk_foreign_marshalling_supported(void) {
  return ON1X_SDK_UNSUPPORTED;
}

On1xSDK_Status on1x_sdk_foreign_module_loading_supported(void) {
  return ON1X_SDK_UNSUPPORTED;
}

On1xSDK_Status on1x_sdk_binding_generator_api_supported(void) {
  return ON1X_SDK_UNSUPPORTED;
}
