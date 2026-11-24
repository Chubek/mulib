/* MuLIB PRIME: imported GEM handles and exported fds are caller-owned. */
#ifndef MULIB_DRM_PRIME_H
#define MULIB_DRM_PRIME_H
#include "mulib-drm-kms.h"
#include <fcntl.h>
#if MULIB_GFX_DRM_UAPI
static inline int mulib_drm_prime_export(int fd, uint32_t handle, int *dmafd)
{
    if (!handle || !dmafd) { errno = EINVAL; return -1; }
    /* Strict C11 feature visibility can hide O_CLOEXEC. Never guess the target
     * architecture's flag or silently fall back to a racy fd export. */
#if defined(O_CLOEXEC)
    struct drm_prime_handle q = {0};
    q.handle = handle; q.fd = -1;
    q.flags = (uint32_t)(O_CLOEXEC | O_RDWR);
    if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &q) < 0) return -1;
    if (q.fd < 0) { errno = EIO; return -1; }
    *dmafd = q.fd; return 0;
#else
    (void)fd;
    errno = ENOTSUP; return -1;
#endif
}

static inline int mulib_drm_prime_import(int fd, int dmafd, uint32_t *handle)
{
    struct drm_prime_handle q = {0};
    if (dmafd < 0 || !handle) { errno = EINVAL; return -1; }
    q.fd = dmafd;
    if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_PRIME_FD_TO_HANDLE, &q) < 0) return -1;
    *handle = q.handle; return 0;
}
#endif
#endif
