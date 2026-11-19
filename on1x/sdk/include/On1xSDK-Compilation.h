#ifndef ON1X_SDK_COMPILATION_H
#define ON1X_SDK_COMPILATION_H

#include "On1xSDK-AST.h"
#include "On1xSDK-Options.h"
#include "On1xSDK-VFS.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct On1xSDK_CompilationInfo {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t revision;
    uint64_t session_revision;
    uint64_t vfs_revision;
    uint64_t entry_source_id;
    uint64_t entry_source_revision;
    size_t dependency_count;
    uint32_t is_current;
    uint32_t reserved;
} On1xSDK_CompilationInfo;

/*
 * Uses the canonical module loader for physical entry paths and every VFS
 * overlay as an exact immutable source override. A virtual non-file entry is
 * supported only when it has no imports.
 */
On1xSDK_Status on1x_sdk_compilation_create(
    On1xSDK_Session *session,
    On1xSDK_VFS *vfs,
    const On1xSDK_Options *options,
    On1xSDK_Source *entry,
    On1xSDK_Compilation **out_compilation);
On1xSDK_Status on1x_sdk_compilation_retain(
    On1xSDK_Compilation *compilation);
void on1x_sdk_compilation_release(On1xSDK_Compilation *compilation);
On1xSDK_Status on1x_sdk_compilation_info(
    const On1xSDK_Compilation *compilation,
    On1xSDK_CompilationInfo *out_info);
On1xSDK_Status on1x_sdk_compilation_ast(
    const On1xSDK_Compilation *compilation,
    On1xSDK_AST **out_ast);
On1xSDK_Status on1x_sdk_compilation_is_current(
    const On1xSDK_Compilation *compilation,
    uint32_t *out_current);

#ifdef __cplusplus
}
#endif

#endif
