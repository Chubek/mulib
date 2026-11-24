/* MuLIB GEM handle ioctls; no implicit handle/fd ownership transfer. */
#ifndef MULIB_DRM_GEM_H
#define MULIB_DRM_GEM_H
#include "mulib-drm-kms.h"
#if MULIB_GFX_DRM_UAPI
static inline int mulib_drm_gem_close(int fd, uint32_t id)
{
    struct drm_gem_close q = {0};
    if (!id) { errno = EINVAL; return -1; }
    q.handle = id;
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_GEM_CLOSE, &q);
}

static inline int mulib_drm_gem_flink(int fd, uint32_t handle, uint32_t *name)
{
    struct drm_gem_flink q = {0};
    if (!handle || !name) { errno = EINVAL; return -1; }
    q.handle = handle;
    if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_GEM_FLINK, &q) < 0) return -1;
    *name = q.name; return 0;
}

static inline int mulib_drm_gem_open(int fd, uint32_t name, uint32_t *handle,
                                   uint64_t *bytes)
{
    struct drm_gem_open q = {0};
    if (!name || !handle) { errno = EINVAL; return -1; }
    q.name = name;
    if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_GEM_OPEN, &q) < 0) return -1;
    *handle = q.handle;
    if (bytes) *bytes = q.size;
    return 0;
}
#endif
#endif
