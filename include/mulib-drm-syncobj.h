/* MuLIB binary syncobj helpers. Driver capability/permission errors propagate. */
#ifndef MULIB_DRM_SYNCOBJ_H
#define MULIB_DRM_SYNCOBJ_H
#include "mulib-drm-kms.h"
#if MULIB_GFX_DRM_UAPI && defined(DRM_IOCTL_SYNCOBJ_CREATE)
static inline int mulib_drm_syncobj_create(int fd, uint32_t flags, uint32_t *id)
{
    struct drm_syncobj_create q = {0};
    if (!id) { errno = EINVAL; return -1; }
    q.flags = flags;
    if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_SYNCOBJ_CREATE, &q) < 0) return -1;
    *id = q.handle; return 0;
}

static inline int mulib_drm_syncobj_destroy(int fd, uint32_t id)
{
    struct drm_syncobj_destroy q = {0};
    if (!id) { errno = EINVAL; return -1; }
    q.handle = id;
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_SYNCOBJ_DESTROY, &q);
}

/* absolute_ns is an absolute CLOCK_MONOTONIC deadline, not a relative timeout. */
static inline int mulib_drm_syncobj_wait(int fd, const uint32_t *handles,
                                        uint32_t n, int64_t absolute_ns,
                                        uint32_t flags)
{
    struct drm_syncobj_wait q = {0};
    if (!handles || !n) { errno = EINVAL; return -1; }
    q.handles = (uintptr_t)handles; q.count_handles = n;
    q.timeout_nsec = absolute_ns; q.flags = flags;
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_SYNCOBJ_WAIT, &q);
}

static inline int mulib_drm_syncobj_signal(int fd, const uint32_t *h, uint32_t n)
{
    struct drm_syncobj_array q = {0};
    if (!h || !n) { errno = EINVAL; return -1; }
    q.handles = (uintptr_t)h; q.count_handles = n;
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_SYNCOBJ_SIGNAL, &q);
}
#else
/* No substitute syncobj ABI on older/missing UAPI. */
static inline int mulib_drm_syncobj_create(int fd, uint32_t flags, uint32_t *id)
{ (void)fd; (void)flags; (void)id; errno = ENOTSUP; return -1; }
static inline int mulib_drm_syncobj_destroy(int fd, uint32_t id)
{ (void)fd; (void)id; errno = ENOTSUP; return -1; }
static inline int mulib_drm_syncobj_wait(int fd, const uint32_t *h, uint32_t n,
                                        int64_t ns, uint32_t flags)
{ (void)fd; (void)h; (void)n; (void)ns; (void)flags; errno = ENOTSUP; return -1; }
static inline int mulib_drm_syncobj_signal(int fd, const uint32_t *h, uint32_t n)
{ (void)fd; (void)h; (void)n; errno = ENOTSUP; return -1; }
#endif
#endif
