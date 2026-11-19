/* MuLIB legacy DRM backend. Caller owns an exclusively acquired DRM session.
 * Not safe to use against an active desktop's DRM master. */
#ifndef MULIB_GLX_BACKEND_DRM_H
#define MULIB_GLX_BACKEND_DRM_H
#include "mulib-drm-framebuffer.h"
#include "mulib-drm-pageflip.h"
#include "mulib-drm-output.h"
#include "mulib-glx-platform.h"
#if MULIB_GFX_DRM_UAPI && defined(__has_include)
#if __has_include("mulib-drm.h")
#include "mulib-drm.h"
#define MULIB_GFX_DRM_BACKEND 1

/* Initialize to zero; do not copy. device/fd must outlive release. All connector
 * routing for the chosen CRTC is saved. A connector already routed to a different
 * CRTC is rejected: restoring multiple CRTCs requires an atomic session backend. */
typedef struct {
    mulib_drm_device_t *device;
    mulib_drm_dumb_t buffers[2];
    mgx_surface maps[2];
    uint32_t fb[2], crtc, connector;
    struct drm_mode_crtc old;
    struct drm_mode_modeinfo mode;
    unsigned front;
    int active, pending;
    uint32_t *old_connectors;
    size_t old_connector_count;
} mulib_glx_backend_drm_t;

/* Returns 0/-1, retaining unreleased resources on errors for retry. Completion
 * must first be dispatched; even restoring a CRTC is unsafe with an in-flight
 * flip. A restore failure never removes live framebuffers/dumb handles. */
static inline int mulib_glx_backend_drm_release_checked(mulib_glx_backend_drm_t *b)
{
    int fd;
    if (!b) { errno = EINVAL; return -1; }
    if (!b->device) return 0;
    if (b->pending) { errno = EBUSY; return -1; }
    fd = b->device->fd;
    if (b->active) {
        if (mulib_drm_kms_restore(fd, &b->old, b->old_connectors,
                                  b->old_connector_count) < 0) return -1;
        b->active = 0;
    }
    for (size_t i = 0; i < 2; ++i) {
        if (b->fb[i]) {
            if (mulib_drm_framebuffer_remove(fd, b->fb[i]) < 0) return -1;
            b->fb[i] = 0;
        }
        /* The surface is a borrowed view, never a separate allocation. */
        if (mulib_drm_dumb_destroy(&b->buffers[i]) != MULIB_DRM_OK) return -1;
        memset(&b->maps[i], 0, sizeof b->maps[i]);
    }
    free(b->old_connectors);
    memset(b, 0, sizeof *b);
    b->buffers[0].fd = b->buffers[1].fd = -1;
    return 0;
}

/* Legacy void wrapper; inspect errno or prefer release_checked for reporting. */
static inline void mulib_glx_backend_drm_release(mulib_glx_backend_drm_t *b)
{
    if (b) (void)mulib_glx_backend_drm_release_checked(b);
}

static inline int mulib_glx_backend_drm_init(mulib_glx_backend_drm_t *b,
                                            mulib_drm_device_t *d, uint32_t c,
                                            uint32_t crtc,
                                            const struct drm_mode_modeinfo *m)
{
    mulib_drm_kms_resources_t resources = {0};
    mulib_drm_kms_connector_t selected = {0};
    int saved, compatible;
    if (!b || !d || d->fd < 0 || !c || !crtc || !m ||
        !m->hdisplay || !m->vdisplay) { errno = EINVAL; return 0; }
    if (b->device) { errno = EBUSY; return 0; }
#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    /* Surface storage is native uint32_t; this path has not negotiated BE KMS. */
    errno = ENOTSUP; return 0;
#endif
    memset(b, 0, sizeof *b);
    b->buffers[0].fd = b->buffers[1].fd = -1;
    b->device = d; b->connector = c; b->crtc = crtc; b->mode = *m;
    if (mulib_drm_kms_resources(d->fd, &resources) < 0 ||
        mulib_drm_kms_connector(d->fd, c, &selected) < 0) goto fail;
    {
        size_t i;
        for (i = 0; i < resources.res.count_connectors; ++i)
            if (resources.connectors[i] == c) break;
        if (i == resources.res.count_connectors) { errno = EAGAIN; goto fail; }
    }
    if (selected.info.connection != 1) { errno = ENODEV; goto fail; }
    compatible = mulib_drm_output_compatible(d->fd, &resources, &selected, crtc);
    if (compatible < 0) goto fail;
    if (!compatible) { errno = EINVAL; goto fail; }
    if (selected.info.encoder_id) {
        struct drm_mode_get_encoder e;
        if (mulib_drm_kms_encoder(d->fd, selected.info.encoder_id, &e) < 0) goto fail;
        if (e.crtc_id && e.crtc_id != crtc) { errno = EBUSY; goto fail; }
    }
    if (mulib_drm_kms_crtc(d->fd, crtc, &b->old) < 0) goto fail;
    b->old_connectors = calloc(resources.res.count_connectors ?
                               resources.res.count_connectors : 1,
                               sizeof *b->old_connectors);
    if (!b->old_connectors) { errno = ENOMEM; goto fail; }
    for (size_t i = 0; i < resources.res.count_connectors; ++i) {
        mulib_drm_kms_connector_t other = {0};
        struct drm_mode_get_encoder e = {0};
        if (mulib_drm_kms_connector(d->fd, resources.connectors[i], &other) < 0)
            goto fail;
        if (other.info.encoder_id &&
            mulib_drm_kms_encoder(d->fd, other.info.encoder_id, &e) < 0) {
            saved = errno; mulib_drm_kms_connector_free(&other);
            errno = saved; goto fail;
        }
        if (e.crtc_id == crtc)
            b->old_connectors[b->old_connector_count++] = other.info.connector_id;
        mulib_drm_kms_connector_free(&other);
    }
    if (b->old.mode_valid && !b->old_connector_count) {
        errno = EAGAIN; goto fail;
    }
    mulib_drm_kms_connector_free(&selected);
    mulib_drm_kms_resources_free(&resources);
    for (size_t i = 0; i < 2; ++i) {
        if (mulib_drm_dumb_create(d, m->hdisplay, m->vdisplay, 32, &b->buffers[i])
            != MULIB_DRM_OK) goto fail;
        if (mulib_drm_dumb_map(&b->buffers[i]) != MULIB_DRM_OK) goto fail;
        if (b->buffers[i].pitch % sizeof(uint32_t)) { errno = EIO; goto fail; }
        if (mulib_drm_framebuffer_add(d->fd, m->hdisplay, m->vdisplay,
            DRM_FORMAT_XRGB8888, b->buffers[i].handle, b->buffers[i].pitch,
            &b->fb[i]) < 0) goto fail;
        if (!mgx_wrap(&b->maps[i], b->buffers[i].data,
            b->buffers[i].pitch / sizeof(uint32_t), m->hdisplay, m->vdisplay)) {
            errno = EOVERFLOW; goto fail;
        }
        mgx_clear(&b->maps[i], 0xff000000);
    }
    if (mulib_drm_kms_set(d->fd, crtc, b->fb[0], &c, 1, m) < 0) goto fail;
    b->active = 1;
    return 1;
fail:
    saved = errno;
    mulib_drm_kms_connector_free(&selected);
    mulib_drm_kms_resources_free(&resources);
    /* If cleanup itself fails, b remains owning and must be released again. */
    (void)mulib_glx_backend_drm_release_checked(b);
    errno = saved;
    return 0;
}

static inline void mulib_glx_backend_drm_flip_done(void *u, uint64_t cookie,
                                                  uint32_t seq, uint32_t sec,
                                                  uint32_t usec)
{
    mulib_glx_backend_drm_t *b = u;
    (void)seq; (void)sec; (void)usec;
    if (b && cookie == (uint64_t)(uintptr_t)b && b->pending) {
        b->front ^= 1; b->pending = 0;
    }
}

/* Full-frame copy into the non-front buffer. Advertise MGX_CAP_ASYNC_PRESENT
 * through the platform; after dispatch clears pending, the caller acknowledges
 * the actual completion through its window/present completion API. */
static inline int mulib_glx_backend_drm_present(void *u, const mgx_surface *s,
                                               const mgx_rect *r, size_t n)
{
    mulib_glx_backend_drm_t *b = u;
    mgx_surface *dst;
    if (!b || !b->device || !b->active || !s || !s->pixels || s->w <= 0 ||
        s->h <= 0 || s->stride < (size_t)s->w ||
        s->stride > SIZE_MAX / (size_t)s->h / sizeof(uint32_t) ||
        (n && !r) || s->w != b->maps[0].w || s->h != b->maps[0].h) {
        errno = EINVAL; return 0;
    }
    if (b->pending) { errno = EBUSY; return 0; }
    dst = &b->maps[b->front ^ 1];
    for (int y = 0; y < s->h; ++y)
        memmove(dst->pixels + (size_t)y * dst->stride,
                s->pixels + (size_t)y * s->stride,
                (size_t)s->w * sizeof(uint32_t));
    if (mulib_drm_pageflip(b->device->fd, b->crtc, b->fb[b->front ^ 1],
                           (uintptr_t)b) < 0) return 0;
    b->pending = 1; return 1;
}

static inline int mulib_glx_backend_drm_dispatch(mulib_glx_backend_drm_t *b)
{
    if (!b || !b->device) { errno = EINVAL; return -1; }
    return mulib_drm_pageflip_read(b->device->fd, mulib_glx_backend_drm_flip_done, b);
}
#endif
#endif
#ifndef MULIB_GFX_DRM_BACKEND
#define MULIB_GFX_DRM_BACKEND 0
#endif
#endif
