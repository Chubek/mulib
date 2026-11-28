#ifndef ON1X_SDK_PASS_H
#define ON1X_SDK_PASS_H

#include "On1xSDK-Query.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_PassKind;
enum {
    ON1X_SDK_PASS_CONSTANT_FOLD = 0,
    ON1X_SDK_PASS_SIMPLIFY_CFG = 1,
    ON1X_SDK_PASS_DEAD_PURE = 2,
    ON1X_SDK_PASS_LOCAL_SLOTS = 3
};
enum {
    ON1X_SDK_PRESERVE_SIGNATURES = 1u << 0,
    ON1X_SDK_PRESERVE_CFG = 1u << 1,
    ON1X_SDK_PRESERVE_CALLS = 1u << 2,
    ON1X_SDK_PRESERVE_EFFECTS = 1u << 3,
    ON1X_SDK_PRESERVE_VALUES = 1u << 4,
    ON1X_SDK_PRESERVE_LIVENESS = 1u << 5
};

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    On1xSDK_PassKind pass;
    uint32_t pass_version;
    On1xSDK_AnalysisStringView name;
    uint32_t preserves_mask;
    uint32_t invalidates_mask;
    On1xSDK_AnalysisStringView target_profile;
    On1xSDK_AnalysisStringView verifier;
} On1xSDK_PassDescriptor;

On1xSDK_Status on1x_sdk_pass_descriptor(
    On1xSDK_PassKind pass, On1xSDK_PassDescriptor *out_descriptor);

#ifdef __cplusplus
}
#endif

#endif
