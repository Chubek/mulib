/* MuLIB atomic request construction; not an end-to-end atomic KMS backend. */
#ifndef MULIB_DRM_ATOMIC_H
#define MULIB_DRM_ATOMIC_H
#include "mulib-drm-kms.h"
#include "mulib-glx-core.h"
#if MULIB_GFX_DRM_UAPI
typedef struct { uint32_t object, property; uint64_t value; } mulib_drm_atomic_item;
typedef struct { mulib_drm_atomic_item *v; size_t n, cap; } mulib_drm_atomic_t;

static inline int mulib_drm_atomic_enable(int fd)
{
    struct drm_set_client_cap q = {0};
    q.capability = DRM_CLIENT_CAP_ATOMIC; q.value = 1;
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_SET_CLIENT_CAP, &q);
}

/* Builder uses 1/0 success; output is unchanged if allocation/validation fails. */
static inline int mulib_drm_atomic_add(mulib_drm_atomic_t *a, uint32_t object,
                                      uint32_t prop, uint64_t value)
{
    if (!a || !object || !prop || a->n > a->cap || (a->cap && !a->v)) {
        errno = EINVAL; return 0;
    }
    for (size_t i = 0; i < a->n; ++i)
        if (a->v[i].object == object && a->v[i].property == prop) {
            a->v[i].value = value; return 1;
        }
    if (a->n >= UINT32_MAX || a->n >= SIZE_MAX / sizeof *a->v) {
        errno = EOVERFLOW; return 0;
    }
    if (!mgx_grow((void **)&a->v, &a->cap, a->n + 1, sizeof *a->v)) {
        errno = ENOMEM; return 0;
    }
    a->v[a->n++] = (mulib_drm_atomic_item){object, prop, value};
    return 1;
}

static inline int mulib_drm_atomic_commit(int fd, const mulib_drm_atomic_t *a,
                                         uint32_t flags, uint64_t cookie)
{
    uint32_t *objs, *counts, *props;
    uint64_t *vals;
    struct drm_mode_atomic q = {0};
    size_t n, no = 0, k = 0;
    int rc = -1, saved;
    if (!a || !a->v || !a->n || a->n > a->cap) { errno = EINVAL; return -1; }
    n = a->n;
    if (n > UINT32_MAX || n > SIZE_MAX / sizeof *vals) {
        errno = EOVERFLOW; return -1;
    }
    objs = calloc(n, sizeof *objs); counts = calloc(n, sizeof *counts);
    props = calloc(n, sizeof *props); vals = calloc(n, sizeof *vals);
    if (!objs || !counts || !props || !vals) { errno = ENOMEM; goto done; }
    for (size_t i = 0; i < n; ++i) {
        size_t j = 0;
        if (!a->v[i].object || !a->v[i].property) { errno = EINVAL; goto done; }
        while (j < no && objs[j] != a->v[i].object) ++j;
        if (j < no) continue;
        objs[no] = a->v[i].object;
        for (size_t t = i; t < n; ++t)
            if (a->v[t].object == objs[no]) {
                props[k] = a->v[t].property; vals[k++] = a->v[t].value;
                ++counts[no];
            }
        ++no;
    }
    q.flags = flags; q.count_objs = (uint32_t)no;
    q.objs_ptr = (uintptr_t)objs; q.count_props_ptr = (uintptr_t)counts;
    q.props_ptr = (uintptr_t)props; q.prop_values_ptr = (uintptr_t)vals;
    q.user_data = cookie;
    rc = mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_ATOMIC, &q);
done:
    saved = errno; free(objs); free(counts); free(props); free(vals);
    errno = saved; return rc;
}

static inline int mulib_drm_atomic_test(int fd, const mulib_drm_atomic_t *a)
{
    return mulib_drm_atomic_commit(fd, a,
        DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET, 0);
}

/* Blob IDs are caller-owned until blob_free succeeds. Outputs unchanged on error. */
static inline int mulib_drm_atomic_blob(int fd, const void *data, uint32_t n,
                                       uint32_t *id)
{
    struct drm_mode_create_blob q = {0};
    if (!data || !n || !id) { errno = EINVAL; return -1; }
    q.data = (uintptr_t)data; q.length = n;
    if (mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_CREATEPROPBLOB, &q) < 0) return -1;
    *id = q.blob_id; return 0;
}

static inline int mulib_drm_atomic_blob_free(int fd, uint32_t id)
{
    struct drm_mode_destroy_blob q = {0};
    if (!id) { errno = EINVAL; return -1; }
    q.blob_id = id;
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_DESTROYPROPBLOB, &q);
}

static inline int mulib_drm_atomic_properties(int fd,
                                             struct drm_mode_obj_get_properties *q)
{
    if (!q || !q->obj_id ||
        (q->count_props && (!q->props_ptr || !q->prop_values_ptr))) {
        errno = EINVAL; return -1;
    }
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, q);
}

static inline int mulib_drm_atomic_property(int fd, struct drm_mode_get_property *q)
{
    if (!q || !q->prop_id || (q->count_values && !q->values_ptr) ||
        (q->count_enum_blobs && !q->enum_blob_ptr)) { errno = EINVAL; return -1; }
    return mulib_drm_kms_ioctl(fd, DRM_IOCTL_MODE_GETPROPERTY, q);
}

/* Resolve by actual kernel property name, not fixed IDs. Does not infer enum
 * meanings or manufacture missing properties. ENOENT if absent, EAGAIN on churn. */
static inline int mulib_drm_atomic_property_find(int fd, uint32_t object,
                                                uint32_t type, const char *name,
                                                uint32_t *id, uint64_t *value)
{
    struct drm_mode_get_property prop = {0};
    size_t name_len = 0;
    if (!object || !name || !id) { errno = EINVAL; return -1; }
    while (name_len < sizeof prop.name && name[name_len]) ++name_len;
    if (!name_len || name_len == sizeof prop.name) { errno = EINVAL; return -1; }
    for (int attempt = 0; attempt < 4; ++attempt) {
        struct drm_mode_obj_get_properties q = {0};
        uint32_t *ids, count;
        uint64_t *values;
        int saved;
        q.obj_id = object; q.obj_type = type;
        if (mulib_drm_atomic_properties(fd, &q) < 0) return -1;
        count = q.count_props;
        if (count > 65536) { errno = EOVERFLOW; return -1; }
        ids = calloc(count ? count : 1, sizeof *ids);
        values = calloc(count ? count : 1, sizeof *values);
        if (!ids || !values) {
            free(ids); free(values); errno = ENOMEM; return -1;
        }
        q.props_ptr = (uintptr_t)ids; q.prop_values_ptr = (uintptr_t)values;
        if (mulib_drm_atomic_properties(fd, &q) < 0) {
            saved = errno; free(ids); free(values); errno = saved; return -1;
        }
        if (q.count_props > count) { free(ids); free(values); continue; }
        for (size_t i = 0; i < q.count_props; ++i) {
            memset(&prop, 0, sizeof prop); prop.prop_id = ids[i];
            if (mulib_drm_atomic_property(fd, &prop) < 0) {
                saved = errno; free(ids); free(values); errno = saved; return -1;
            }
            if (!strncmp(prop.name, name, sizeof prop.name)) {
                *id = ids[i];
                if (value) *value = values[i];
                free(ids); free(values); return 0;
            }
        }
        free(ids); free(values); errno = ENOENT; return -1;
    }
    errno = EAGAIN; return -1;
}

static inline void mulib_drm_atomic_free(mulib_drm_atomic_t *a)
{
    if (a) { free(a->v); memset(a, 0, sizeof *a); }
}
#endif
#endif
