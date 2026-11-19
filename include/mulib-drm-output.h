/* MuLIB DRM output helpers. No seat acquisition or implicit modesetting. */
#ifndef MULIB_DRM_OUTPUT_H
#define MULIB_DRM_OUTPUT_H
#include "mulib-drm-kms.h"
#if MULIB_GFX_DRM_UAPI
static inline int mulib_drm_output_preferred(const mulib_drm_kms_connector_t *c,
                                            struct drm_mode_modeinfo *out)
{
    size_t best = SIZE_MAX;
    if (!c || !out || !c->modes || c->info.connection != 1) return 0;
    for (size_t i = 0; i < c->info.count_modes; ++i) {
        if (!c->modes[i].hdisplay || !c->modes[i].vdisplay) continue;
        if (best == SIZE_MAX ||
            (uint64_t)c->modes[i].hdisplay * c->modes[i].vdisplay >
            (uint64_t)c->modes[best].hdisplay * c->modes[best].vdisplay)
            best = i;
        if (c->modes[i].type & DRM_MODE_TYPE_PREFERRED) { best = i; break; }
    }
    if (best == SIZE_MAX) return 0;
    *out = c->modes[best]; return 1;
}

static inline double mulib_drm_output_dpi(const mulib_drm_kms_connector_t *c,
                                         const struct drm_mode_modeinfo *m)
{
    return c && m && c->info.mm_width ? 25.4 * m->hdisplay / c->info.mm_width : 0;
}

/* possible_crtcs bits are indices into resources.crtcs, never DRM object IDs.
 * Encoder query errors are not disguised as "no matching output". */
static inline int mulib_drm_output_compatible(int fd,
                                             const mulib_drm_kms_resources_t *r,
                                             const mulib_drm_kms_connector_t *c,
                                             uint32_t crtc)
{
    size_t index;
    if (!r || !c || !crtc || (r->res.count_crtcs && !r->crtcs) ||
        (c->info.count_encoders && !c->encoders)) { errno = EINVAL; return -1; }
    for (index = 0; index < r->res.count_crtcs; ++index)
        if (r->crtcs[index] == crtc) break;
    if (index >= r->res.count_crtcs || index >= 32) return 0;
    for (size_t i = 0; i < c->info.count_encoders; ++i) {
        struct drm_mode_get_encoder e;
        if (mulib_drm_kms_encoder(fd, c->encoders[i], &e) < 0) return -1;
        if (e.possible_crtcs & (UINT32_C(1) << index)) return 1;
    }
    return 0;
}

/* Find a compatible CRTC not currently routed to another connector and not in
 * caller's reserved list. The connector's own current CRTC may be reused.
 * Returns 0/-1, out unchanged on failure (ENOSPC means no compatible free CRTC).
 * Snapshot + exclusive seat ownership is required; this is not a reservation. */
static inline int mulib_drm_output_find_crtc(int fd,
                                            const mulib_drm_kms_resources_t *r,
                                            const mulib_drm_kms_connector_t *c,
                                            const uint32_t *reserved, size_t n,
                                            uint32_t *out)
{
    uint32_t busy = 0;
    if (!r || !c || !out || (n && !reserved) ||
        (r->res.count_connectors && !r->connectors) ||
        (r->res.count_crtcs && !r->crtcs) || c->info.connection != 1) {
        errno = EINVAL; return -1;
    }
    for (size_t i = 0; i < r->res.count_connectors; ++i) {
        mulib_drm_kms_connector_t other = {0};
        struct drm_mode_get_encoder e;
        int saved;
        if (r->connectors[i] == c->info.connector_id) continue;
        if (mulib_drm_kms_connector(fd, r->connectors[i], &other) < 0) return -1;
        if (!other.info.encoder_id) {
            mulib_drm_kms_connector_free(&other); continue;
        }
        if (mulib_drm_kms_encoder(fd, other.info.encoder_id, &e) < 0) {
            saved = errno; mulib_drm_kms_connector_free(&other);
            errno = saved; return -1;
        }
        mulib_drm_kms_connector_free(&other);
        for (size_t j = 0; j < r->res.count_crtcs && j < 32; ++j)
            if (r->crtcs[j] == e.crtc_id) busy |= UINT32_C(1) << j;
    }
    for (size_t i = 0; i < r->res.count_crtcs && i < 32; ++i) {
        int in_use = (busy & (UINT32_C(1) << i)) != 0, compatible;
        for (size_t j = 0; j < n; ++j)
            if (reserved[j] == r->crtcs[i]) in_use = 1;
        if (in_use) continue;
        compatible = mulib_drm_output_compatible(fd, r, c, r->crtcs[i]);
        if (compatible < 0) return -1;
        if (compatible) { *out = r->crtcs[i]; return 0; }
    }
    errno = ENOSPC; return -1;
}
#endif

static inline int mulib_drm_output_edid_valid(const unsigned char *p, size_t n)
{
    static const unsigned char magic[8] = {0,255,255,255,255,255,255,0};
    if (!p || n < 128 || memcmp(p, magic, sizeof magic) ||
        n / 128 < (size_t)p[126] + 1) return 0;
    for (size_t b = 0; b <= (size_t)p[126]; ++b) {
        unsigned sum = 0;
        for (size_t i = 0; i < 128; ++i) sum += p[b * 128 + i];
        if (sum & 255) return 0;
    }
    return 1;
}
#endif
