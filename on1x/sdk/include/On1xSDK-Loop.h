#ifndef ON1X_SDK_LOOP_H
#define ON1X_SDK_LOOP_H

#include "On1xSDK-AnalysisManager.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ON1X_SDK_NO_PARENT_LOOP UINT64_MAX

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t function_id;
    uint64_t loop_ordinal;
    uint64_t header_block_id; /* Zero for an irreducible SCC. */
    uint64_t latch_block_id;  /* Zero for an irreducible SCC. */
    uint64_t parent_loop_ordinal;
    uint32_t is_reducible;
    uint32_t reserved;
} On1xSDK_Loop;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t function_id;
    uint64_t loop_ordinal;
    uint64_t block_id;
} On1xSDK_LoopMember;

size_t on1x_sdk_loop_count(const On1xSDK_AnalysisManager *manager,
                           const On1xSDK_AnalysisResult *result);
size_t on1x_sdk_loop_member_count(const On1xSDK_AnalysisManager *manager,
                                  const On1xSDK_AnalysisResult *result);
On1xSDK_Status on1x_sdk_loop_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result, size_t index,
    On1xSDK_Loop *out_loop);
On1xSDK_Status on1x_sdk_loop_member_at(
    const On1xSDK_AnalysisManager *manager,
    const On1xSDK_AnalysisResult *result, size_t index,
    On1xSDK_LoopMember *out_member);

#ifdef __cplusplus
}
#endif

#endif
