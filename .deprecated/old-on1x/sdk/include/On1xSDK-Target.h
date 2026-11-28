/* Immutable, canonical target profiles. */
#ifndef ON1X_SDK_TARGET_H
#define ON1X_SDK_TARGET_H

#include "On1xSDK-CallingConvention.h"
#include "On1xSDK-DataLayout.h"
#include "On1xSDK-ObjectABI.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_MAX_TARGET_BYTES 128u
#define ON1X_SDK_MAX_CPU_BYTES 64u
#define ON1X_SDK_MAX_TARGET_FEATURE_BYTES 64u
#define ON1X_SDK_MAX_TARGET_FEATURES 64u

typedef struct On1xSDK_Target On1xSDK_Target;

typedef struct On1xSDK_TargetFeature {
    const char *data;
    size_t size;
} On1xSDK_TargetFeature;

typedef struct On1xSDK_TargetDescriptor {
    uint32_t abi_version;
    uint32_t struct_size;

    /* Canonical lower-case triple with three to five nonempty components. */
    const char *triple;
    size_t triple_size;
    /* Optional lower-case CPU identity. Empty is the generic CPU. */
    const char *cpu;
    size_t cpu_size;

    On1xSDK_DataLayout data_layout;
    On1xSDK_ObjectFormat object_format;
    On1xSDK_CallingConvention calling_convention;

    /* Copied, canonicalized into lexical order, and required to be unique. */
    const On1xSDK_TargetFeature *features;
    size_t feature_count;
} On1xSDK_TargetDescriptor;

/*
 * Creation validates the complete descriptor and known architecture/profile
 * invariants. It does not claim that a backend or toolchain supports the
 * target. Success transfers one immutable owner to the caller.
 */
On1xSDK_Status on1x_sdk_target_create(const On1xSDK_TargetDescriptor *descriptor,
                                      On1xSDK_Target **out_target);
void on1x_sdk_target_destroy(On1xSDK_Target *target);

/*
 * The descriptor and all of its views are borrowed from target and remain
 * valid until target destruction. The returned descriptor has ABI version 2
 * and the complete current structure size.
 */
On1xSDK_Status on1x_sdk_target_view(const On1xSDK_Target *target,
                                    const On1xSDK_TargetDescriptor **out_descriptor);
On1xSDK_Status on1x_sdk_target_has_feature(const On1xSDK_Target *target,
                                           const char *feature, size_t feature_size,
                                           uint32_t *out_present);
On1xSDK_Status on1x_sdk_target_equal(const On1xSDK_Target *left,
                                     const On1xSDK_Target *right,
                                     uint32_t *out_equal);

#ifdef __cplusplus
}
#endif

#endif
