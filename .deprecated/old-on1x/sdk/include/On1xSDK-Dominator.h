#ifndef ON1X_SDK_DOMINATOR_H
#define ON1X_SDK_DOMINATOR_H

#include "On1xSDK-AnalysisManager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t function_id;
    uint64_t block_id;
    uint64_t immediate_dominator_id; /* Zero only for an entry block. */
    uint64_t depth;
} On1xSDK_Dominator;

size_t on1x_sdk_dominator_count(const On1xSDK_AnalysisManager *manager,
                                const On1xSDK_AnalysisResult *result);
On1xSDK_Status on1x_sdk_dominator_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result, size_t index,
    On1xSDK_Dominator *out_dominator);

#ifdef __cplusplus
}
#endif

#endif
