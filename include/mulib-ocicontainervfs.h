/** @file mulib-ocicontainervfs.h
 * @brief Mount a memory rootfs into a VFS with explicit read-only policy.
 * Define MULIB_IMPL_OCICONTAINERVFS plus VFS. Backend context remains
 * borrowed. */
#ifndef MULIB_OCICONTAINERVFS_H
#define MULIB_OCICONTAINERVFS_H
#include "mulib-rootfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Mount root storage, disabling writes when readonly=1. 0 success.
   * root must outlive the mount; no host mounts, chroot or namespace
   * operations. */
  int mulib_ocicontainervfs_mount (mulib_vfs *vfs, const char *path,
                                   mulib_rootfs_t *root, int readonly);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OCICONTAINERVFS)                                       \
    && !defined(MULIB_OCICONTAINERVFS_IMPLEMENTED)
#define MULIB_OCICONTAINERVFS_IMPLEMENTED
int
mulib_ocicontainervfs_mount (mulib_vfs *v, const char *path, mulib_rootfs_t *r,
                             int ro)
{
  mulib_vfs_ops ops;
  if (!r || !r->storage.max_nodes || (ro != 0 && ro != 1))
    return MULIB_VFS_EINVAL;
  ops = mulib_vfs_mem_ops ();
  if (ro)
    ops.write = NULL;
  return mulib_vfs_mount (v, path, &ops, &r->storage);
}
#endif
