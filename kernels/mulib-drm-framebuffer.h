/* MuLIB single-plane framebuffer registration. No format/modifier negotiation. */
#ifndef MULIB_DRM_FRAMEBUFFER_H
#define MULIB_DRM_FRAMEBUFFER_H
#include "mulib-drm-kms.h"
#if MULIB_GFX_DRM_UAPI
static inline int mulib_drm_framebuffer_add(int fd, uint32_t width, uint32_t height,
                                           uint32_t format, uint32_t handle,
                                           uint32_t pitch, uint32_t *fb)
{
    struct drm_mode_fb_cmd2 q = {0};
    if (!fb || !width || !height || !format || !handle || !pitch) {
        errno = EINVAL; return -1;
    }
    /* Known packed formats must fit their pitch. Other single-plane formats
     * are validated by the kernel; planar formats need a different API. */
    if (format == DRM_FORMAT_XRGB8888 || format == DRM_FORMAT_ARGB8888) {
        if ((uint64_t)width * 4 > UINT32_MAX) { errno = EOVERFLOW; return -1; }
        if (pitch < (uint64_t)width * 4) { errno = EINVAL; return -1; }
    }
    q.width = width; q.height = height; q.pixel_format = format;
    q.handles[0] = handle; q.pitches[0] = pitch;
    if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_ADDFB2, &q) < 0) return -1;
    *fb = q.fb_id; return 0;
}

static inline int mulib_drm_framebuffer_remove(int fd, uint32_t fb)
{
    if (!fb) { errno = EINVAL; return -1; }
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_RMFB, &fb);
}
#endif
#endif
