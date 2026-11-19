#ifndef ON1X_SDK_CFG_H
#define ON1X_SDK_CFG_H

#include "On1xSDK-AnalysisManager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t function_id;
    uint64_t block_id;
    uint64_t predecessor_count;
    uint64_t successor_count;
    uint32_t is_entry;
    uint32_t reserved;
} On1xSDK_CFGBlock;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t function_id;
    uint64_t source_block_id;
    uint64_t target_block_id;
    uint64_t terminator_id;
    uint64_t edge_ordinal;
} On1xSDK_CFGEdge;

size_t on1x_sdk_cfg_block_count(const On1xSDK_AnalysisManager *manager,
                                const On1xSDK_AnalysisResult *result);
size_t on1x_sdk_cfg_edge_count(const On1xSDK_AnalysisManager *manager,
                               const On1xSDK_AnalysisResult *result);
On1xSDK_Status on1x_sdk_cfg_block_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result, size_t index,
    On1xSDK_CFGBlock *out_block);
On1xSDK_Status on1x_sdk_cfg_edge_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result, size_t index,
    On1xSDK_CFGEdge *out_edge);

#ifdef __cplusplus
}
#endif

#endif
