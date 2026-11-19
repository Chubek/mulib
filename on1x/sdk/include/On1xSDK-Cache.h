#ifndef ON1X_SDK_CACHE_H
#define ON1X_SDK_CACHE_H

#include "On1xSDK-Options.h"
#include "On1xSDK-Parser.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_CacheInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    size_t capacity;
    size_t entry_count;
    uint64_t hits;
    uint64_t misses;
} On1xSDK_CacheInfo;

On1xSDK_Status on1x_sdk_cache_create(
    On1xSDK_Session *session,
    size_t capacity,
    On1xSDK_Cache **out_cache);
On1xSDK_Status on1x_sdk_cache_retain(On1xSDK_Cache *cache);
void on1x_sdk_cache_release(On1xSDK_Cache *cache);
/* Keys compare the exact source revision and complete options revision/value. */
On1xSDK_Status on1x_sdk_cache_store(
    On1xSDK_Cache *cache,
    const On1xSDK_SourceRevision *source,
    const On1xSDK_Options *options,
    const On1xSDK_AST *ast);
On1xSDK_Status on1x_sdk_cache_lookup(
    On1xSDK_Cache *cache,
    const On1xSDK_SourceRevision *source,
    const On1xSDK_Options *options,
    On1xSDK_AST **out_ast);
On1xSDK_Status on1x_sdk_cache_clear(On1xSDK_Cache *cache);
On1xSDK_Status on1x_sdk_cache_info(
    const On1xSDK_Cache *cache,
    On1xSDK_CacheInfo *out_info);

#ifdef __cplusplus
}
#endif

#endif
