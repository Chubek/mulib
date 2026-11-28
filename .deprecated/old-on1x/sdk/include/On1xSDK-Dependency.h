#ifndef ON1X_SDK_DEPENDENCY_H
#define ON1X_SDK_DEPENDENCY_H

#include "On1xSDK-Compilation.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_DependencyInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    size_t index;
    uint64_t source_id;
    uint64_t source_revision;
    const char *path;
    size_t path_size;
    size_t byte_size;
    uint32_t is_entry;
    uint32_t from_overlay;
} On1xSDK_DependencyInfo;

/* Requires CAPTURE_MODULE_GRAPH. logical=0: physical imports, 1: logical
 * dependencies. Returned UTF-8 JSON bytes are borrowed until compilation
 * release; later workspace edits do not mutate this observation. */
On1xSDK_Status on1x_sdk_compilation_graph_json_v1(
    const On1xSDK_Compilation *compilation, uint32_t logical,
    const char **out_bytes, size_t *out_size);

/* Dependencies are canonical, deduplicated, and dependency-first. */
On1xSDK_Status on1x_sdk_compilation_dependency_count(
    const On1xSDK_Compilation *compilation,
    size_t *out_count);
On1xSDK_Status on1x_sdk_compilation_dependency(
    const On1xSDK_Compilation *compilation,
    size_t index,
    On1xSDK_DependencyInfo *out_dependency);
On1xSDK_Status on1x_sdk_compilation_dependency_source(
    const On1xSDK_Compilation *compilation,
    size_t index,
    On1xSDK_SourceSnapshot **out_revision);

#ifdef __cplusplus
}
#endif

#endif
