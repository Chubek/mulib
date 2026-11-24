/* MuLIB legacy page flips. Caller owns the fd and drains real completion events. */
#ifndef MULIB_DRM_PAGEFLIP_H
#define MULIB_DRM_PAGEFLIP_H
#include "mulib-drm-kms.h"
#include <limits.h>
#if MULIB_GFX_DRM_UAPI
static inline int mulib_drm_pageflip(int fd, uint32_t crtc, uint32_t fb,
                                    uint64_t cookie)
{
    struct drm_mode_crtc_page_flip q = {0};
    if (!crtc || !fb) { errno = EINVAL; return -1; }
    q.crtc_id = crtc; q.fb_id = fb;
    q.flags = DRM_MODE_PAGE_FLIP_EVENT; q.user_data = cookie;
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_PAGE_FLIP, &q);
}

typedef void (*mulib_drm_pageflip_fn)(void *, uint64_t, uint32_t, uint32_t, uint32_t);

/* Validate the whole read before callbacks: malformed trailers have no effects.
 * Unknown events and vblank notifications are skipped, not flip completions.
 * Callback must not modify/free the borrowed event buffer during parsing.
 * Returns number of flip completions, or -1/EINVAL, EPROTO, EOVERFLOW. */
static inline int mulib_drm_pageflip_parse(const void *buf, size_t n,
                                          mulib_drm_pageflip_fn fn, void *user)
{
    const unsigned char *p = buf;
    size_t offset = 0;
    int count = 0;
    if (n && !buf) { errno = EINVAL; return -1; }
    while (offset < n) {
        struct drm_event h;
        if (n - offset < sizeof h) { errno = EPROTO; return -1; }
        memcpy(&h, p + offset, sizeof h);
        if (h.length < sizeof h || h.length > n - offset) {
            errno = EPROTO; return -1;
        }
        if (h.type == DRM_EVENT_FLIP_COMPLETE || h.type == DRM_EVENT_VBLANK) {
            if (h.length < sizeof(struct drm_event_vblank)) {
                errno = EPROTO; return -1;
            }
            if (h.type == DRM_EVENT_FLIP_COMPLETE) {
                if (count == INT_MAX) { errno = EOVERFLOW; return -1; }
                ++count;
            }
        }
        offset += h.length;
    }
    for (offset = 0; offset < n;) {
        struct drm_event h;
        memcpy(&h, p + offset, sizeof h);
        if (h.type == DRM_EVENT_FLIP_COMPLETE && fn) {
            struct drm_event_vblank v;
            memcpy(&v, p + offset, sizeof v);
            fn(user, v.user_data, v.sequence, v.tv_sec, v.tv_usec);
        }
        offset += h.length;
    }
    return count;
}

static inline int mulib_drm_pageflip_read(int fd, mulib_drm_pageflip_fn fn, void *u)
{
    unsigned char b[4096];
    ssize_t n;
    do { n = read(fd, b, sizeof b); } while (n < 0 && errno == EINTR);
    if (n < 0) return -1;
    if (!n) { errno = EPIPE; return -1; }
    return mulib_drm_pageflip_parse(b, (size_t)n, fn, u);
}
#endif
#endif
