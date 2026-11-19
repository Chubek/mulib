/* MuLIB DRM/KMS: Linux UAPI only; no libdrm or implicit device access. */
#ifndef MULIB_DRM_KMS_H
#define MULIB_DRM_KMS_H
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

/* Defining this to zero explicitly disables the optional UAPI. */
#if !defined(MULIB_GFX_DRM_UAPI) || MULIB_GFX_DRM_UAPI
# if defined(__linux__) && defined(__has_include)
#  if __has_include(<drm/drm.h>) && __has_include(<drm/drm_mode.h>) && __has_include(<drm/drm_fourcc.h>)
#   include <drm/drm.h>
#   include <drm/drm_mode.h>
#   include <drm/drm_fourcc.h>
#   define MULIB_GFX_DRM_UAPI_FOUND 1
#  elif __has_include(<libdrm/drm.h>) && __has_include(<libdrm/drm_mode.h>) && __has_include(<libdrm/drm_fourcc.h>)
#   include <libdrm/drm.h>
#   include <libdrm/drm_mode.h>
#   include <libdrm/drm_fourcc.h>
#   define MULIB_GFX_DRM_UAPI_FOUND 1
#  endif
# endif
# ifdef MULIB_GFX_DRM_UAPI
#  undef MULIB_GFX_DRM_UAPI
# endif
# ifdef MULIB_GFX_DRM_UAPI_FOUND
#  define MULIB_GFX_DRM_UAPI 1
# else
#  define MULIB_GFX_DRM_UAPI 0
# endif
#endif

/* Shared with mulib-drm.h. Define before either header/implementation.
 * No global mutable hook state; every TU must use the same desired hook. */
#ifndef MULIB_DRM_IOCTL
#define MULIB_DRM_IOCTL(fd, op, arg) ioctl((fd), (op), (arg))
#endif

static inline int mulib_drm_kms_supported(void)
{
    return MULIB_GFX_DRM_UAPI;
}

static inline int mulib_drm_kms_ioctl(int fd, unsigned long op, void *arg)
{
#if MULIB_GFX_DRM_UAPI
    int rc;
    if (fd < 0) { errno = EBADF; return -1; }
    do { rc = MULIB_DRM_IOCTL(fd, op, arg); } while (rc < 0 && errno == EINTR);
    return rc;
#else
    (void)fd; (void)op; (void)arg;
    errno = ENOTSUP;
    return -1;
#endif
}

#if MULIB_GFX_DRM_UAPI
/* Owning snapshots: initialize to zero, do not copy, free before reuse.
 * Fetches leave out unchanged on failure. Bounded retries handle hotplug. */
typedef struct {
    struct drm_mode_card_res res;
    uint32_t *fbs, *crtcs, *connectors, *encoders;
} mulib_drm_kms_resources_t;

static inline void mulib_drm_kms_resources_free(mulib_drm_kms_resources_t *r)
{
    if (!r) return;
    free(r->fbs); free(r->crtcs); free(r->connectors); free(r->encoders);
    memset(r, 0, sizeof *r);
}

static inline int mulib_drm_kms_resources(int fd, mulib_drm_kms_resources_t *out)
{
    if (!out) { errno = EINVAL; return -1; }
    for (int attempt = 0; attempt < 4; ++attempt) {
        mulib_drm_kms_resources_t r = {0};
        uint32_t f, c, n, e;
        int saved;
        if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &r.res) < 0)
            return -1;
        f = r.res.count_fbs; c = r.res.count_crtcs;
        n = r.res.count_connectors; e = r.res.count_encoders;
        if (f > 65536 || c > 65536 || n > 65536 || e > 65536) {
            errno = EOVERFLOW; return -1;
        }
        r.fbs = calloc(f ? f : 1, sizeof *r.fbs);
        r.crtcs = calloc(c ? c : 1, sizeof *r.crtcs);
        r.connectors = calloc(n ? n : 1, sizeof *r.connectors);
        r.encoders = calloc(e ? e : 1, sizeof *r.encoders);
        if (!r.fbs || !r.crtcs || !r.connectors || !r.encoders) {
            mulib_drm_kms_resources_free(&r); errno = ENOMEM; return -1;
        }
        r.res.fb_id_ptr = (uintptr_t)r.fbs;
        r.res.crtc_id_ptr = (uintptr_t)r.crtcs;
        r.res.connector_id_ptr = (uintptr_t)r.connectors;
        r.res.encoder_id_ptr = (uintptr_t)r.encoders;
        if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &r.res) < 0) {
            saved = errno; mulib_drm_kms_resources_free(&r);
            errno = saved; return -1;
        }
        if (r.res.count_fbs <= f && r.res.count_crtcs <= c &&
            r.res.count_connectors <= n && r.res.count_encoders <= e) {
            *out = r; return 0;
        }
        mulib_drm_kms_resources_free(&r);
    }
    errno = EAGAIN; return -1;
}

typedef struct {
    struct drm_mode_get_connector info;
    struct drm_mode_modeinfo *modes;
    uint32_t *encoders, *props;
    uint64_t *values;
} mulib_drm_kms_connector_t;

static inline void mulib_drm_kms_connector_free(mulib_drm_kms_connector_t *r)
{
    if (!r) return;
    free(r->modes); free(r->encoders); free(r->props); free(r->values);
    memset(r, 0, sizeof *r);
}

static inline int mulib_drm_kms_connector(int fd, uint32_t id,
                                         mulib_drm_kms_connector_t *out)
{
    if (!id || !out) { errno = EINVAL; return -1; }
    for (int attempt = 0; attempt < 4; ++attempt) {
        mulib_drm_kms_connector_t r = {0};
        uint32_t m, e, p;
        int saved;
        r.info.connector_id = id;
        if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &r.info) < 0)
            return -1;
        m = r.info.count_modes; e = r.info.count_encoders; p = r.info.count_props;
        if (m > 65536 || e > 65536 || p > 65536) {
            errno = EOVERFLOW; return -1;
        }
        r.modes = calloc(m ? m : 1, sizeof *r.modes);
        r.encoders = calloc(e ? e : 1, sizeof *r.encoders);
        r.props = calloc(p ? p : 1, sizeof *r.props);
        r.values = calloc(p ? p : 1, sizeof *r.values);
        if (!r.modes || !r.encoders || !r.props || !r.values) {
            mulib_drm_kms_connector_free(&r); errno = ENOMEM; return -1;
        }
        r.info.modes_ptr = (uintptr_t)r.modes;
        r.info.encoders_ptr = (uintptr_t)r.encoders;
        r.info.props_ptr = (uintptr_t)r.props;
        r.info.prop_values_ptr = (uintptr_t)r.values;
        if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &r.info) < 0) {
            saved = errno; mulib_drm_kms_connector_free(&r);
            errno = saved; return -1;
        }
        if (r.info.count_modes <= m && r.info.count_encoders <= e &&
            r.info.count_props <= p) { *out = r; return 0; }
        mulib_drm_kms_connector_free(&r);
    }
    errno = EAGAIN; return -1;
}

static inline int mulib_drm_kms_encoder(int fd, uint32_t id,
                                       struct drm_mode_get_encoder *e)
{
    struct drm_mode_get_encoder q = {0};
    if (!id || !e) { errno = EINVAL; return -1; }
    q.encoder_id = id;
    if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_GETENCODER, &q) < 0) return -1;
    *e = q; return 0;
}

static inline int mulib_drm_kms_crtc(int fd, uint32_t id, struct drm_mode_crtc *c)
{
    struct drm_mode_crtc q = {0};
    if (!id || !c) { errno = EINVAL; return -1; }
    q.crtc_id = id;
    if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_GETCRTC, &q) < 0) return -1;
    *c = q; return 0;
}

/* Restore preserves the saved viewport x/y as well as every connector route. */
static inline int mulib_drm_kms_restore(int fd, const struct drm_mode_crtc *saved,
                                       const uint32_t *connectors, size_t n)
{
    struct drm_mode_crtc q;
    if (!saved || !saved->crtc_id || (n && !connectors)) {
        errno = EINVAL; return -1;
    }
    if (n > UINT32_MAX) { errno = EOVERFLOW; return -1; }
    q = *saved;
    q.set_connectors_ptr = (uintptr_t)connectors;
    q.count_connectors = (uint32_t)n;
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_SETCRTC, &q);
}

static inline int mulib_drm_kms_set(int fd, uint32_t crtc, uint32_t fb,
                                   const uint32_t *connectors, size_t n,
                                   const struct drm_mode_modeinfo *mode)
{
    struct drm_mode_crtc q = {0};
    q.crtc_id = crtc; q.fb_id = fb;
    if (mode) { q.mode = *mode; q.mode_valid = 1; }
    return mulib_drm_kms_restore(fd, &q, connectors, n);
}

/* On success n is the current count, potentially larger than input capacity.
 * Caller must retry enumeration rather than read beyond its supplied array. */
static inline int mulib_drm_kms_planes(int fd, uint32_t *ids, uint32_t *n)
{
    struct drm_mode_get_plane_res q = {0};
    if (!n) { errno = EINVAL; return -1; }
    q.count_planes = ids ? *n : 0; q.plane_id_ptr = (uintptr_t)ids;
    if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_GETPLANERESOURCES, &q) < 0) return -1;
    *n = q.count_planes; return 0;
}

static inline int mulib_drm_kms_plane(int fd, struct drm_mode_get_plane *p)
{
    if (!p || !p->plane_id || (p->count_format_types && !p->format_type_ptr)) {
        errno = EINVAL; return -1;
    }
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_GETPLANE, p);
}
#endif
#endif
