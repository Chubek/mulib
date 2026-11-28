#ifndef ON1X_SDK_WORKSPACE_H
#define ON1X_SDK_WORKSPACE_H

#include "On1xSDK-Compilation.h"

#ifdef __cplusplus
extern "C" {
#endif

/* root must name an existing directory; resolved children may not escape it. */
On1xSDK_Status on1x_sdk_workspace_create(
    On1xSDK_Session *session,
    const char *root,
    size_t root_size,
    const On1xSDK_Options *options,
    On1xSDK_Workspace **out_workspace);
On1xSDK_Status on1x_sdk_workspace_retain(On1xSDK_Workspace *workspace);
void on1x_sdk_workspace_release(On1xSDK_Workspace *workspace);

/* Creates/replaces a workspace overlay and returns one retained source. */
On1xSDK_Status on1x_sdk_workspace_overlay(
    On1xSDK_Workspace *workspace,
    const char *relative_path,
    size_t relative_path_size,
    const void *bytes,
    size_t byte_size,
    On1xSDK_SourceEncoding encoding,
    On1xSDK_Source **out_source);
On1xSDK_Status on1x_sdk_workspace_open(
    On1xSDK_Workspace *workspace,
    const char *relative_path,
    size_t relative_path_size,
    On1xSDK_SourceEncoding encoding,
    On1xSDK_Source **out_source);
On1xSDK_Status on1x_sdk_workspace_compile(
    On1xSDK_Workspace *workspace,
    const char *relative_path,
    size_t relative_path_size,
    On1xSDK_Compilation **out_compilation);
On1xSDK_Status on1x_sdk_workspace_vfs(
    const On1xSDK_Workspace *workspace,
    On1xSDK_VFS **out_vfs);
On1xSDK_Status on1x_sdk_workspace_options(
    const On1xSDK_Workspace *workspace,
    On1xSDK_Options **out_options);

#ifdef __cplusplus
}
#endif

#endif
