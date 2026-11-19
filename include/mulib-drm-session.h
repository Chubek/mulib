/* Explicit caller-owned DRM/VT operations, not a logind/seat/session manager. */
#ifndef MULIB_DRM_SESSION_H
#define MULIB_DRM_SESSION_H
#include "mulib-drm-kms.h"
#include <fcntl.h>
#if defined(__linux__)
#include <linux/vt.h>
#include <linux/kd.h>
#endif

static inline int mulib_drm_session_master(int fd, int take)
{
#if MULIB_GFX_DRM_UAPI
    return mulib_drm_kms_ioctl(fd,
        take ? DRM_IOCTL_SET_MASTER : DRM_IOCTL_DROP_MASTER, NULL);
#else
    (void)fd; (void)take; errno = ENOTSUP; return -1;
#endif
}

#if defined(__linux__)
/* VT ioctls take an integer value, unlike pointer-valued DRM requests.
 * Explicit caller-owned tty only; no discovery, permissions or seat policy. */
#ifndef MULIB_DRM_VT_IOCTL
#define MULIB_DRM_VT_IOCTL(fd, op, value) ioctl((fd), (op), (value))
#endif
static inline int mulib_drm_session_vt_mode(int tty, int graphics)
{
    int rc;
    if (tty < 0) { errno = EBADF; return -1; }
    do {
        rc = MULIB_DRM_VT_IOCTL(tty, KDSETMODE, graphics ? KD_GRAPHICS : KD_TEXT);
    } while (rc < 0 && errno == EINTR);
    return rc;
}

static inline int mulib_drm_session_vt_ack(int tty, int acquire)
{
    int rc;
    if (tty < 0) { errno = EBADF; return -1; }
    do {
        rc = MULIB_DRM_VT_IOCTL(tty, VT_RELDISP, acquire ? VT_ACKACQ : 1);
    } while (rc < 0 && errno == EINTR);
    return rc;
}
#else
static inline int mulib_drm_session_vt_mode(int tty, int graphics)
{ (void)tty; (void)graphics; errno = ENOTSUP; return -1; }
static inline int mulib_drm_session_vt_ack(int tty, int acquire)
{ (void)tty; (void)acquire; errno = ENOTSUP; return -1; }
#endif
#endif
