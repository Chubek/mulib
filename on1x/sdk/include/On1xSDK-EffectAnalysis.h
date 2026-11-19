#ifndef ON1X_SDK_EFFECT_ANALYSIS_H
#define ON1X_SDK_EFFECT_ANALYSIS_H

#include "On1xSDK-AnalysisManager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_MIREffect;
enum {
    ON1X_SDK_MIR_EFFECT_PURE = 0,
    ON1X_SDK_MIR_EFFECT_TRAP = 1,
    ON1X_SDK_MIR_EFFECT_MEMORY = 2,
    ON1X_SDK_MIR_EFFECT_CALL = 3,
    ON1X_SDK_MIR_EFFECT_IO = 4
};
enum {
    ON1X_SDK_EFFECT_MASK_TRAP = 1u << 0,
    ON1X_SDK_EFFECT_MASK_MEMORY = 1u << 1,
    ON1X_SDK_EFFECT_MASK_CALL = 1u << 2,
    ON1X_SDK_EFFECT_MASK_IO = 1u << 3
};

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t function_id;
    uint64_t block_id;
    uint64_t instruction_id;
    On1xSDK_MIREffect effect;
    uint32_t reserved;
} On1xSDK_EffectStep;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t function_id;
    uint32_t transitive_effect_mask;
    uint32_t reserved;
} On1xSDK_FunctionEffects;

size_t on1x_sdk_effect_step_count(const On1xSDK_AnalysisManager *manager,
                                  const On1xSDK_AnalysisResult *result);
size_t on1x_sdk_function_effect_count(const On1xSDK_AnalysisManager *manager,
                                      const On1xSDK_AnalysisResult *result);
On1xSDK_Status on1x_sdk_effect_step_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result, size_t index,
    On1xSDK_EffectStep *out_effect);
On1xSDK_Status on1x_sdk_function_effect_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result, size_t index,
    On1xSDK_FunctionEffects *out_effects);

#ifdef __cplusplus
}
#endif

#endif
