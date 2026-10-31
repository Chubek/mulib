/** @file mulib-ocicontainervolume.h
 * @brief Validated declarative bind-volume specifications; no host mount
 * calls. Define MULIB_IMPL_OCICONTAINERVOLUME plus VFS. */
#ifndef MULIB_OCICONTAINERVOLUME_H
#define MULIB_OCICONTAINERVOLUME_H
#include "mulib-vfs.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_ocicontainervolume
  {
    char source[MULIB_VFS_PATH_CAP], target[MULIB_VFS_PATH_CAP];
    int readonly;
  } mulib_ocicontainervolume_t;
  /** @brief Normalize absolute source/target, reject root target and
   * above-root paths. 1 success, unchanged on error. Lexical validation is NOT
   * symlink confinement; a privileged backend must separately enforce
   * filesystem/race/security policy. */
  int mulib_ocicontainervolume_init (mulib_ocicontainervolume_t *out,
                                     const char *source, const char *target,
                                     int readonly);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_OCICONTAINERVOLUME)                                    \
    && !defined(MULIB_OCICONTAINERVOLUME_IMPLEMENTED)
#define MULIB_OCICONTAINERVOLUME_IMPLEMENTED
#include <string.h>
int
mulib_ocicontainervolume_init (mulib_ocicontainervolume_t *out,
                               const char *source, const char *target, int ro)
{
  mulib_ocicontainervolume_t v;
  if (!out || (ro != 0 && ro != 1)
      || mulib_vfs_normalize (source, v.source, sizeof (v.source))
      || mulib_vfs_normalize (target, v.target, sizeof (v.target))
      || !strcmp (v.target, "/"))
    return 0;
  v.readonly = ro;
  *out = v;
  return 1;
}
#endif
