#ifndef ON1X_SDK_LIVENESS_H
#define ON1X_SDK_LIVENESS_H

#include "On1xSDK-AnalysisManager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t On1xSDK_DefinitionKind;
enum {
    ON1X_SDK_DEFINITION_BLOCK_PARAMETER = 1,
    ON1X_SDK_DEFINITION_INSTRUCTION = 2
};
typedef uint32_t On1xSDK_UseKind;
enum {
    ON1X_SDK_USE_INSTRUCTION_OPERAND = 1,
    ON1X_SDK_USE_TERMINATOR_VALUE = 2,
    ON1X_SDK_USE_EDGE_ARGUMENT = 3
};

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t function_id;
    uint64_t block_id;
    uint64_t value_id;
    uint64_t owner_id;
    On1xSDK_DefinitionKind kind;
    uint32_t reserved;
} On1xSDK_Definition;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t function_id;
    uint64_t block_id;
    uint64_t value_id;
    uint64_t user_id;
    uint64_t incoming_parameter_id;
    uint64_t edge_ordinal; /* UINT64_MAX unless this is an edge argument. */
    uint64_t operand_ordinal;
    On1xSDK_UseKind kind;
    uint32_t reserved;
} On1xSDK_Use;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t function_id;
    uint64_t block_id;
    uint64_t value_id;
    uint32_t is_live_in;
    uint32_t reserved;
} On1xSDK_LiveValue;

size_t on1x_sdk_definition_count(const On1xSDK_AnalysisManager *manager,
                                 const On1xSDK_AnalysisResult *result);
size_t on1x_sdk_use_count(const On1xSDK_AnalysisManager *manager,
                          const On1xSDK_AnalysisResult *result);
size_t on1x_sdk_live_value_count(const On1xSDK_AnalysisManager *manager,
                                 const On1xSDK_AnalysisResult *result);
On1xSDK_Status on1x_sdk_definition_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result, size_t index,
    On1xSDK_Definition *out_definition);
On1xSDK_Status on1x_sdk_use_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result, size_t index,
    On1xSDK_Use *out_use);
On1xSDK_Status on1x_sdk_live_value_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result, size_t index,
    On1xSDK_LiveValue *out_value);

#ifdef __cplusplus
}
#endif

#endif
