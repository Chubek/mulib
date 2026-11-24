/** @file mulib-containerinjector.h
 * @brief Bounded file insertion into the memory rootfs, not process-code
 * injection. Define MULIB_IMPL_CONTAINERINJECTOR plus VFS. */
#ifndef MULIB_CONTAINERINJECTOR_H
#define MULIB_CONTAINERINJECTOR_H
#include "mulib-rootfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Copy/replace file bytes, requiring existing parent. 0 success, VFS
   * error. Only lexical in-memory paths are used, never native host paths. */
  int mulib_containerinjector_put (mulib_rootfs_t *root, const char *path,
                                   const void *data, size_t size);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_CONTAINERINJECTOR)                                     \
    && !defined(MULIB_CONTAINERINJECTOR_IMPLEMENTED)
#define MULIB_CONTAINERINJECTOR_IMPLEMENTED
int
mulib_containerinjector_put (mulib_rootfs_t *r, const char *p, const void *v,
                             size_t n)
{
  return r ? mulib_vfs_mem_put (&r->storage, p, v, n) : MULIB_VFS_EINVAL;
}
#endif
