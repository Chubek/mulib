#ifndef ON1X_SDK_VFS_H
#define ON1X_SDK_VFS_H

#include "On1xSDK-Source.h"

#ifdef __cplusplus
extern "C" {
#endif

On1xSDK_Status on1x_sdk_vfs_create(
    On1xSDK_Session *session,
    On1xSDK_VFS **out_vfs);
On1xSDK_Status on1x_sdk_vfs_retain(On1xSDK_VFS *vfs);
void on1x_sdk_vfs_release(On1xSDK_VFS *vfs);

/* The VFS retains source. Replacing the same normalized path is atomic. */
On1xSDK_Status on1x_sdk_vfs_set(
    On1xSDK_VFS *vfs,
    On1xSDK_Source *source);
On1xSDK_Status on1x_sdk_vfs_remove(
    On1xSDK_VFS *vfs,
    const char *path,
    size_t path_size);
/* A successful lookup returns one retained source reference. */
On1xSDK_Status on1x_sdk_vfs_lookup(
    const On1xSDK_VFS *vfs,
    const char *path,
    size_t path_size,
    On1xSDK_Source **out_source);
On1xSDK_Status on1x_sdk_vfs_count(
    const On1xSDK_VFS *vfs,
    size_t *out_count);
uint64_t on1x_sdk_vfs_revision(const On1xSDK_VFS *vfs);

#ifdef __cplusplus
}
#endif

#endif
