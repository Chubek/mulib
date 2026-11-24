/* CPU/mock tests only. Never opens a DRM, tty, or input device.
 * cc -std=c11 -Wall -Wextra -Werror -Ikernels tests/test-gfx-drm.c -lm -o test
 * Also compile with -DMULIB_GFX_DRM_UAPI=0 to test the disabled capability. */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int mock_ioctl(int, unsigned long, void *);
static int mock_vt_ioctl(int, unsigned long, int);
#define MULIB_DRM_IOCTL(fd, op, arg) mock_ioctl((fd), (op), (arg))
#define MULIB_DRM_VT_IOCTL(fd, op, value) mock_vt_ioctl((fd), (op), (value))
#define MULIB_IMPL_DRM
#include "mulib-drm.h"
#include "mulib-drm-atomic.h"
#include "mulib-drm-framebuffer.h"
#include "mulib-drm-gem.h"
#include "mulib-drm-output.h"
#include "mulib-drm-pageflip.h"
#include "mulib-drm-prime.h"
#include "mulib-drm-session.h"
#include "mulib-drm-syncobj.h"
#include "mulib-glx-backend-drm.h"

#if MULIB_GFX_DRM_UAPI
/* Validate the preexisting API's native ABI against real target UAPI. */
#define SAME_LAYOUT(a, b) \
    _Static_assert(sizeof(a) == sizeof(b), "native DRM ABI size"); \
    _Static_assert(_Alignof(a) == _Alignof(b), "native DRM ABI alignment")
SAME_LAYOUT(mulib_drm_version_request_t, struct drm_version);
SAME_LAYOUT(mulib_drm_cap_request_t, struct drm_get_cap);
SAME_LAYOUT(mulib_drm_create_request_t, struct drm_mode_create_dumb);
SAME_LAYOUT(mulib_drm_map_request_t, struct drm_mode_map_dumb);
SAME_LAYOUT(mulib_drm_destroy_request_t, struct drm_mode_destroy_dumb);
_Static_assert(offsetof(mulib_drm_version_request_t, name) ==
               offsetof(struct drm_version, name), "native pointer ABI");
_Static_assert(offsetof(mulib_drm_version_request_t, desc_len) ==
               offsetof(struct drm_version, desc_len), "native size_t ABI");

enum { NORMAL, RESOURCE_GROW, RESOURCE_CHURN, RESOURCE_OVERFLOW,
       CONNECTOR_GROW, CONNECTOR_CHURN, PROPERTY_GROW };
struct fault { unsigned long op; unsigned skip, left; int error; };
static struct {
    struct fault faults[2];
    unsigned calls, res_calls, conn_calls, prop_calls;
    unsigned creates, destroys, adds, removes, sets, flips, vt_calls;
    unsigned next_handle, next_fb;
    unsigned live_handles, live_fbs;
    unsigned atomic_calls;
    uint32_t atomic_flags, atomic_objects[4], atomic_counts[4], atomic_props[8];
    uint64_t atomic_values[8], atomic_cookie;
    uint32_t route[3];
    uint32_t last_connectors[3], last_x, last_y;
    size_t last_n;
    int scenario, bad_dumb_pitch;
    unsigned long interrupted_op;
    unsigned interruptions;
    int vt_value;
} mock;

static struct drm_mode_modeinfo test_mode(void)
{
    struct drm_mode_modeinfo m = {0};
    m.hdisplay = 8; m.vdisplay = 4; m.clock = 1000;
    m.htotal = 10; m.vtotal = 5; m.type = DRM_MODE_TYPE_PREFERRED;
    memcpy(m.name, "mock-mode", 10);
    return m;
}

static void reset_mock(void)
{
    memset(&mock, 0, sizeof mock);
    mock.next_handle = 10; mock.next_fb = 1000;
    mock.route[0] = 9001; mock.route[1] = 9001; mock.route[2] = 41;
}

static void inject(unsigned slot, unsigned long op, unsigned skip, int error)
{
    assert(slot < 2);
    mock.faults[slot] = (struct fault){op, skip, 1, error};
}

static int mock_ioctl(int fd, unsigned long op, void *arg)
{
    assert(fd >= 0);
    ++mock.calls;
    if (mock.interrupted_op == op && mock.interruptions) {
        --mock.interruptions; errno = EINTR; return -1;
    }
    for (size_t i = 0; i < 2; ++i) {
        struct fault *f = &mock.faults[i];
        if (f->op != op || !f->left) continue;
        if (f->skip) { --f->skip; continue; }
        --f->left; errno = f->error; return -1;
    }
    if (op == DRM_IOCTL_VERSION) {
        mulib_drm_version_request_t *q = arg;
        q->major = 1; q->minor = 2; q->patchlevel = 3;
        if (q->name && q->name_len >= 4) memcpy(q->name, "mock", 4);
        q->name_len = 4; q->date_len = 0; q->desc_len = 0;
        return 0;
    }
    if (op == DRM_IOCTL_GET_CAP) {
        mulib_drm_cap_request_t *q = arg;
        q->value = q->capability == DRM_CAP_DUMB_BUFFER ? 1 : 0;
        return 0;
    }
    if (op == DRM_IOCTL_MODE_GETRESOURCES) {
        static const uint32_t crtcs[] = {41, 9001, 77};
        static const uint32_t connectors[] = {100, 101, 102};
        static const uint32_t encoders[] = {200, 201, 202};
        struct drm_mode_card_res *q = arg;
        uint32_t capacity = q->count_crtcs, count = 3;
        ++mock.res_calls;
        if ((mock.scenario == RESOURCE_GROW && mock.res_calls == 1) ||
            (mock.scenario == RESOURCE_CHURN && mock.res_calls % 2)) count = 1;
        if (mock.scenario == RESOURCE_OVERFLOW) count = 65537;
        if (q->crtc_id_ptr)
            memcpy((void *)(uintptr_t)q->crtc_id_ptr, crtcs,
                   (capacity < 3 ? capacity : 3) * sizeof *crtcs);
        if (q->connector_id_ptr)
            memcpy((void *)(uintptr_t)q->connector_id_ptr, connectors,
                   (q->count_connectors < 3 ? q->count_connectors : 3) *
                   sizeof *connectors);
        if (q->encoder_id_ptr)
            memcpy((void *)(uintptr_t)q->encoder_id_ptr, encoders,
                   (q->count_encoders < 3 ? q->count_encoders : 3) *
                   sizeof *encoders);
        q->count_fbs = 0; q->count_crtcs = count;
        q->count_connectors = 3; q->count_encoders = 3;
        return 0;
    }
    if (op == DRM_IOCTL_MODE_GETCONNECTOR) {
        struct drm_mode_get_connector *q = arg;
        uint32_t count = 2, capacity = q->count_modes;
        unsigned index;
        assert(q->connector_id >= 100 && q->connector_id <= 102);
        index = q->connector_id - 100;
        ++mock.conn_calls;
        if ((mock.scenario == CONNECTOR_GROW && mock.conn_calls == 1) ||
            (mock.scenario == CONNECTOR_CHURN && mock.conn_calls % 2)) count = 1;
        if (q->modes_ptr) {
            struct drm_mode_modeinfo *m = (void *)(uintptr_t)q->modes_ptr;
            for (size_t i = 0; i < capacity && i < 2; ++i) m[i] = test_mode();
        }
        if (q->encoders_ptr && q->count_encoders)
            *(uint32_t *)(uintptr_t)q->encoders_ptr = 200 + index;
        if (q->props_ptr && q->count_props)
            *(uint32_t *)(uintptr_t)q->props_ptr = 501;
        if (q->prop_values_ptr && q->count_props)
            *(uint64_t *)(uintptr_t)q->prop_values_ptr = 123;
        q->encoder_id = 200 + index; q->connection = 1;
        q->count_modes = count; q->count_encoders = 1; q->count_props = 1;
        q->mm_width = 300;
        return 0;
    }
    if (op == DRM_IOCTL_MODE_GETENCODER) {
        struct drm_mode_get_encoder *q = arg;
        assert(q->encoder_id >= 200 && q->encoder_id <= 202);
        q->crtc_id = mock.route[q->encoder_id - 200];
        q->possible_crtcs = q->encoder_id == 202 ? 1 : 6;
        return 0;
    }
    if (op == DRM_IOCTL_MODE_GETCRTC) {
        struct drm_mode_crtc *q = arg;
        assert(q->crtc_id == 9001);
        q->fb_id = 80; q->x = 3; q->y = 7;
        q->mode_valid = 1; q->mode = test_mode();
        return 0;
    }
    if (op == DRM_IOCTL_MODE_SETCRTC) {
        struct drm_mode_crtc *q = arg;
        const uint32_t *ids = (const void *)(uintptr_t)q->set_connectors_ptr;
        assert(q->crtc_id == 9001 && q->count_connectors <= 3);
        ++mock.sets; mock.last_n = q->count_connectors;
        mock.last_x = q->x; mock.last_y = q->y;
        for (size_t i = 0; i < 3; ++i)
            if (mock.route[i] == q->crtc_id) mock.route[i] = 0;
        for (size_t i = 0; i < q->count_connectors; ++i) {
            assert(ids[i] >= 100 && ids[i] <= 102);
            mock.route[ids[i] - 100] = q->crtc_id;
            mock.last_connectors[i] = ids[i];
        }
        return 0;
    }
    if (op == DRM_IOCTL_MODE_CREATE_DUMB) {
        mulib_drm_create_request_t *q = arg;
        assert(!q->flags && !q->handle && q->bpp == 32);
        q->handle = mock.next_handle++;
        q->pitch = mock.bad_dumb_pitch ? 1 : (q->width * 4 + 63) / 64 * 64;
        q->size = (uint64_t)q->pitch * q->height;
        ++mock.creates; ++mock.live_handles;
        return 0;
    }
    if (op == DRM_IOCTL_MODE_MAP_DUMB) {
        mulib_drm_map_request_t *q = arg;
        assert(q->handle >= 10 && !q->pad);
        q->offset = (uint64_t)(q->handle - 10) * 4096;
        return 0;
    }
    if (op == DRM_IOCTL_MODE_DESTROY_DUMB) {
        mulib_drm_destroy_request_t *q = arg;
        assert(q->handle >= 10 && mock.live_handles);
        ++mock.destroys; --mock.live_handles;
        return 0;
    }
    if (op == DRM_IOCTL_MODE_ADDFB2) {
        struct drm_mode_fb_cmd2 *q = arg;
        assert(q->pixel_format == DRM_FORMAT_XRGB8888 && q->handles[0]);
        assert(!q->handles[1] && !q->pitches[1] && !q->offsets[0] &&
               !q->modifier[0] && !q->flags);
        q->fb_id = mock.next_fb++;
        ++mock.adds; ++mock.live_fbs;
        return 0;
    }
    if (op == DRM_IOCTL_MODE_RMFB) {
        assert(*(uint32_t *)arg >= 1000 && mock.live_fbs);
        ++mock.removes; --mock.live_fbs;
        return 0;
    }
    if (op == DRM_IOCTL_MODE_PAGE_FLIP) {
        struct drm_mode_crtc_page_flip *q = arg;
        assert(q->crtc_id == 9001 && q->fb_id >= 1000);
        assert(q->flags == DRM_MODE_PAGE_FLIP_EVENT && !q->reserved);
        ++mock.flips; return 0;
    }
    if (op == DRM_IOCTL_MODE_ATOMIC) {
        struct drm_mode_atomic *q = arg;
        const uint32_t *counts = (const void *)(uintptr_t)q->count_props_ptr;
        size_t n = 0;
        assert(q->count_objs <= 4 && !q->reserved);
        ++mock.atomic_calls; mock.atomic_flags = q->flags;
        mock.atomic_cookie = q->user_data;
        for (size_t i = 0; i < q->count_objs; ++i) n += counts[i];
        assert(n <= 8);
        memcpy(mock.atomic_objects, (void *)(uintptr_t)q->objs_ptr,
               q->count_objs * sizeof(uint32_t));
        memcpy(mock.atomic_counts, counts, q->count_objs * sizeof(uint32_t));
        memcpy(mock.atomic_props, (void *)(uintptr_t)q->props_ptr,
               n * sizeof(uint32_t));
        memcpy(mock.atomic_values, (void *)(uintptr_t)q->prop_values_ptr,
               n * sizeof(uint64_t));
        return 0;
    }
    if (op == DRM_IOCTL_MODE_OBJ_GETPROPERTIES) {
        struct drm_mode_obj_get_properties *q = arg;
        const uint32_t ids[] = {501, 502};
        const uint64_t values[] = {111, 222};
        ++mock.prop_calls;
        assert(q->obj_id == 9001 && q->obj_type == DRM_MODE_OBJECT_CRTC);
        if (q->props_ptr) {
            size_t n = q->count_props < 2 ? q->count_props : 2;
            memcpy((void *)(uintptr_t)q->props_ptr, ids, n * sizeof *ids);
            memcpy((void *)(uintptr_t)q->prop_values_ptr, values, n * sizeof *values);
        }
        q->count_props = mock.scenario == PROPERTY_GROW && mock.prop_calls == 1 ? 1 : 2;
        return 0;
    }
    if (op == DRM_IOCTL_MODE_GETPROPERTY) {
        struct drm_mode_get_property *q = arg;
        assert(q->prop_id == 501 || q->prop_id == 502);
        memcpy(q->name, q->prop_id == 501 ? "ACTIVE" : "CRTC_ID",
               q->prop_id == 501 ? 7 : 8);
        return 0;
    }
    if (op == DRM_IOCTL_MODE_CREATEPROPBLOB) {
        struct drm_mode_create_blob *q = arg;
        assert(q->length == sizeof(struct drm_mode_modeinfo) && q->data);
        q->blob_id = 600; return 0;
    }
    if (op == DRM_IOCTL_MODE_DESTROYPROPBLOB) {
        assert(((struct drm_mode_destroy_blob *)arg)->blob_id == 600); return 0;
    }
    if (op == DRM_IOCTL_PRIME_HANDLE_TO_FD) {
        struct drm_prime_handle *q = arg;
        assert(q->handle == 10 && q->flags == (uint32_t)(O_CLOEXEC | O_RDWR));
        q->fd = fcntl(fd, F_DUPFD_CLOEXEC, 0);
        return q->fd < 0 ? -1 : 0;
    }
    if (op == DRM_IOCTL_PRIME_FD_TO_HANDLE) {
        struct drm_prime_handle *q = arg;
        assert(q->fd >= 0 && !q->flags);
        q->handle = 700; return 0;
    }
    if (op == DRM_IOCTL_GEM_FLINK) {
        struct drm_gem_flink *q = arg;
        assert(q->handle == 700 && !q->name);
        q->name = 701; return 0;
    }
    if (op == DRM_IOCTL_GEM_OPEN) {
        struct drm_gem_open *q = arg;
        assert(q->name == 701);
        q->handle = 702; q->size = UINT64_C(1) << 40; return 0;
    }
    if (op == DRM_IOCTL_GEM_CLOSE) {
        assert(((struct drm_gem_close *)arg)->handle == 702); return 0;
    }
    if (op == DRM_IOCTL_SYNCOBJ_CREATE) {
        struct drm_syncobj_create *q = arg;
        assert(q->flags == DRM_SYNCOBJ_CREATE_SIGNALED);
        q->handle = 800; return 0;
    }
    if (op == DRM_IOCTL_SYNCOBJ_WAIT) {
        struct drm_syncobj_wait *q = arg;
        assert(q->count_handles == 1 && q->timeout_nsec == INT64_MAX);
        assert(*(uint32_t *)(uintptr_t)q->handles == 800);
        return 0;
    }
    if (op == DRM_IOCTL_SYNCOBJ_SIGNAL) {
        struct drm_syncobj_array *q = arg;
        assert(q->count_handles == 1 && !q->pad);
        assert(*(uint32_t *)(uintptr_t)q->handles == 800); return 0;
    }
    if (op == DRM_IOCTL_SYNCOBJ_DESTROY) {
        assert(((struct drm_syncobj_destroy *)arg)->handle == 800); return 0;
    }
    if (op == DRM_IOCTL_SET_CLIENT_CAP) {
        struct drm_set_client_cap *q = arg;
        assert(q->capability == DRM_CLIENT_CAP_ATOMIC && q->value == 1); return 0;
    }
    if (op == DRM_IOCTL_SET_MASTER || op == DRM_IOCTL_DROP_MASTER) {
        assert(!arg); return 0;
    }
    assert(!"unexpected mock ioctl");
    errno = ENOTTY; return -1;
}

static int mock_vt_ioctl(int fd, unsigned long op, int value)
{
    assert(fd == 123 && (op == KDSETMODE || op == VT_RELDISP));
    ++mock.vt_calls; mock.vt_value = value;
    if (mock.vt_calls == 1) { errno = EINTR; return -1; }
    return 0;
}

static int temporary_file(char path[64])
{
    int fd;
    strcpy(path, "/tmp/opencode/mulib-gfx-drm-XXXXXX");
    fd = mkstemp(path);
    assert(fd >= 0 && ftruncate(fd, 65536) == 0);
    return fd;
}

static void test_native(int fd, const char *path)
{
    mulib_drm_device_t device = {fd}, opened = MULIB_DRM_DEVICE_INIT;
    mulib_drm_dumb_t buffer = MULIB_DRM_DUMB_INIT;
    mulib_drm_version_t version = {0};
    uint64_t cap = 99;
    reset_mock();
    assert(mulib_drm_ioctl_request(MULIB_DRM_VERSION) == DRM_IOCTL_VERSION);
    assert(mulib_drm_ioctl_request(MULIB_DRM_GET_CAP) == DRM_IOCTL_GET_CAP);
    assert(mulib_drm_ioctl_request(MULIB_DRM_CREATE_DUMB) == DRM_IOCTL_MODE_CREATE_DUMB);
    assert(mulib_drm_ioctl_request(MULIB_DRM_MAP_DUMB) == DRM_IOCTL_MODE_MAP_DUMB);
    assert(mulib_drm_ioctl_request(MULIB_DRM_DESTROY_DUMB) == DRM_IOCTL_MODE_DESTROY_DUMB);
    assert(!mulib_drm_ioctl_request((enum mulib_drm_operation)999));
    assert(mulib_drm_open(&opened, path) == MULIB_DRM_OK);
    assert(fcntl(opened.fd, F_GETFD) & FD_CLOEXEC);
    assert(mulib_drm_close(&opened) == MULIB_DRM_OK && opened.fd == -1);
    mock.interrupted_op = DRM_IOCTL_GET_CAP; mock.interruptions = 2;
    assert(mulib_drm_get_cap(&device, DRM_CAP_DUMB_BUFFER, &cap) == MULIB_DRM_OK);
    assert(cap == 1 && mock.calls == 3);
    inject(0, DRM_IOCTL_GET_CAP, 0, ENOTTY);
    cap = 99;
    assert(mulib_drm_get_cap(&device, 1, &cap) == MULIB_DRM_EIO);
    assert(errno == ENOTTY && cap == 99);
    assert(mulib_drm_get_version(&device, &version) == MULIB_DRM_OK);
    assert(!strcmp(version.name, "mock") && version.major == 1 && version.name_length == 4);
    assert(mulib_drm_dumb_create(&device, UINT32_MAX, 1, 32, &buffer) == MULIB_DRM_EOVERFLOW);
    assert(buffer.fd == -1 && !mock.creates);
    mock.bad_dumb_pitch = 1;
    assert(mulib_drm_dumb_create(&device, 8, 4, 32, &buffer) == MULIB_DRM_EIO);
    assert(buffer.fd == -1 && !mock.live_handles);
    mock.bad_dumb_pitch = 0;
    assert(mulib_drm_dumb_create(&device, 8, 4, 32, &buffer) == MULIB_DRM_OK);
    assert(fcntl(buffer.fd, F_GETFD) & FD_CLOEXEC);
    assert(mulib_drm_dumb_map(&buffer) == MULIB_DRM_OK);
    assert(mulib_drm_dumb_map(&buffer) == MULIB_DRM_OK);
    ((uint32_t *)buffer.data)[0] = 0xff123456;
    inject(0, DRM_IOCTL_MODE_DESTROY_DUMB, 0, EBUSY);
    assert(mulib_drm_dumb_destroy(&buffer) == MULIB_DRM_EIO && errno == EBUSY);
    assert(buffer.fd >= 0 && !buffer.data && !buffer.mapped_size && buffer.handle);
    assert(mulib_drm_dumb_destroy(&buffer) == MULIB_DRM_OK);
    assert(buffer.fd == -1 && !mock.live_handles);
    assert(mulib_drm_dumb_destroy(&buffer) == MULIB_DRM_OK);
}

static void test_enumeration(int fd)
{
    mulib_drm_kms_resources_t r = {0};
    mulib_drm_kms_connector_t c = {0};
    struct drm_mode_modeinfo m = {0};
    struct drm_mode_get_encoder e = {0};
    uint32_t chosen = 123, reserved = 77;
    reset_mock(); mock.scenario = RESOURCE_GROW;
    assert(!mulib_drm_kms_resources(fd, &r) && mock.res_calls == 4);
    assert(r.res.count_crtcs == 3 && r.crtcs[1] == 9001);
    mock.scenario = CONNECTOR_GROW;
    assert(!mulib_drm_kms_connector(fd, 100, &c) && mock.conn_calls == 4);
    assert(c.info.count_modes == 2 && c.encoders[0] == 200 && c.values[0] == 123);
    assert(mulib_drm_output_preferred(&c, &m) && m.hdisplay == 8);
    assert(mulib_drm_output_dpi(&c, &m) > 0);
    assert(mulib_drm_output_compatible(fd, &r, &c, 9001) == 1);
    assert(mulib_drm_output_compatible(fd, &r, &c, 41) == 0);
    assert(!mulib_drm_output_find_crtc(fd, &r, &c, NULL, 0, &chosen) && chosen == 77);
    chosen = 123;
    assert(mulib_drm_output_find_crtc(fd, &r, &c, &reserved, 1, &chosen) == -1);
    assert(errno == ENOSPC && chosen == 123);
    inject(0, DRM_IOCTL_MODE_GETENCODER, 0, ENODEV);
    assert(mulib_drm_kms_encoder(fd, 200, &e) == -1 && errno == ENODEV);
    assert(!e.encoder_id);
    mock.interrupted_op = DRM_IOCTL_MODE_GETENCODER; mock.interruptions = 2;
    assert(!mulib_drm_kms_encoder(fd, 200, &e) && !mock.interruptions);
    assert(e.encoder_id == 200 && e.possible_crtcs == 6);
    assert(mulib_drm_kms_connector(fd, 100, NULL) == -1 && errno == EINVAL);
    assert(mulib_drm_kms_set(fd, 9001, 80, NULL, 1, &m) == -1 && errno == EINVAL);
    if (SIZE_MAX > UINT32_MAX)
        assert(mulib_drm_kms_set(fd, 9001, 80, &chosen, SIZE_MAX, &m) == -1 &&
               errno == EOVERFLOW);
    mulib_drm_kms_connector_free(&c); mulib_drm_kms_resources_free(&r);
    reset_mock(); mock.scenario = RESOURCE_CHURN;
    assert(mulib_drm_kms_resources(fd, &r) == -1 && errno == EAGAIN);
    assert(!r.crtcs && mock.res_calls == 8);
    reset_mock(); mock.scenario = CONNECTOR_CHURN;
    assert(mulib_drm_kms_connector(fd, 100, &c) == -1 && errno == EAGAIN);
    assert(!c.modes && mock.conn_calls == 8);
    reset_mock(); mock.scenario = RESOURCE_OVERFLOW;
    assert(mulib_drm_kms_resources(fd, &r) == -1 && errno == EOVERFLOW);
    reset_mock();
    inject(0, DRM_IOCTL_MODE_GETRESOURCES, 1, ENODEV);
    assert(mulib_drm_kms_resources(fd, &r) == -1 && errno == ENODEV && !r.crtcs);
    inject(0, DRM_IOCTL_MODE_GETCONNECTOR, 1, ENODEV);
    assert(mulib_drm_kms_connector(fd, 100, &c) == -1 && errno == ENODEV && !c.modes);
    mulib_drm_kms_resources_free(NULL); mulib_drm_kms_connector_free(NULL);
}

static void test_atomic(int fd)
{
    mulib_drm_atomic_t a = {0};
    struct drm_mode_modeinfo m = test_mode();
    uint32_t id = 99;
    uint64_t value = 99;
    reset_mock();
    assert(mulib_drm_atomic_enable(fd) == 0);
    assert(!mulib_drm_atomic_add(NULL, 1, 2, 3) && errno == EINVAL);
    assert(mulib_drm_atomic_add(&a, 41, 1, 10));
    assert(mulib_drm_atomic_add(&a, 9001, 2, 20));
    assert(mulib_drm_atomic_add(&a, 41, 3, 30));
    assert(mulib_drm_atomic_add(&a, 41, 1, 11) && a.n == 3);
    assert(!mulib_drm_atomic_commit(fd, &a, DRM_MODE_ATOMIC_NONBLOCK, UINT64_MAX));
    assert(mock.atomic_cookie == UINT64_MAX && mock.atomic_objects[0] == 41);
    assert(mock.atomic_objects[1] == 9001 && mock.atomic_counts[0] == 2 &&
           mock.atomic_counts[1] == 1);
    assert(mock.atomic_props[0] == 1 && mock.atomic_props[1] == 3 &&
           mock.atomic_props[2] == 2);
    assert(mock.atomic_values[0] == 11 && mock.atomic_values[1] == 30 &&
           mock.atomic_values[2] == 20);
    assert(!mulib_drm_atomic_test(fd, &a));
    assert(mock.atomic_flags == (DRM_MODE_ATOMIC_TEST_ONLY | DRM_MODE_ATOMIC_ALLOW_MODESET));
    inject(0, DRM_IOCTL_MODE_ATOMIC, 0, EINVAL);
    assert(mulib_drm_atomic_test(fd, &a) == -1 && errno == EINVAL && a.n == 3);
    mock.scenario = PROPERTY_GROW;
    assert(!mulib_drm_atomic_property_find(fd, 9001, DRM_MODE_OBJECT_CRTC,
                                          "CRTC_ID", &id, &value));
    assert(id == 502 && value == 222 && mock.prop_calls == 4);
    id = 99; value = 99;
    assert(mulib_drm_atomic_property_find(fd, 9001, DRM_MODE_OBJECT_CRTC,
                                         "missing", &id, &value) == -1);
    assert(errno == ENOENT && id == 99 && value == 99);
    assert(mulib_drm_atomic_blob(fd, NULL, sizeof m, &id) == -1 && errno == EINVAL);
    inject(0, DRM_IOCTL_MODE_CREATEPROPBLOB, 0, ENOMEM);
    assert(mulib_drm_atomic_blob(fd, &m, sizeof m, &id) == -1 && id == 99);
    assert(!mulib_drm_atomic_blob(fd, &m, sizeof m, &id) && id == 600);
    inject(0, DRM_IOCTL_MODE_DESTROYPROPBLOB, 0, EBUSY);
    assert(mulib_drm_atomic_blob_free(fd, id) == -1 && errno == EBUSY);
    assert(!mulib_drm_atomic_blob_free(fd, id));
    mulib_drm_atomic_free(&a); mulib_drm_atomic_free(NULL);
    assert(!a.v && !a.n && !a.cap);
}

static void test_handles(int fd)
{
    uint32_t handle = 99, name = 99, sync = 99, fb = 99;
    uint64_t bytes = 99;
    int dmafd = -1;
    reset_mock();
    assert(mulib_drm_prime_export(fd, 10, NULL) == -1 && errno == EINVAL);
    inject(0, DRM_IOCTL_PRIME_HANDLE_TO_FD, 0, ENOTTY);
    assert(mulib_drm_prime_export(fd, 10, &dmafd) == -1 && errno == ENOTTY && dmafd == -1);
    assert(!mulib_drm_prime_export(fd, 10, &dmafd));
    assert(fcntl(dmafd, F_GETFD) & FD_CLOEXEC);
    assert(!mulib_drm_prime_import(fd, dmafd, &handle) && handle == 700);
    assert(!close(dmafd));
    assert(mulib_drm_prime_import(fd, -1, &handle) == -1 && errno == EINVAL);
    assert(mulib_drm_gem_flink(fd, handle, NULL) == -1 && errno == EINVAL);
    assert(!mulib_drm_gem_flink(fd, handle, &name) && name == 701);
    assert(!mulib_drm_gem_open(fd, name, &handle, &bytes));
    assert(handle == 702 && bytes == UINT64_C(1) << 40);
    assert(!mulib_drm_gem_close(fd, handle));
    assert(mulib_drm_syncobj_create(fd, 0, NULL) == -1 && errno == EINVAL);
    assert(!mulib_drm_syncobj_create(fd, DRM_SYNCOBJ_CREATE_SIGNALED, &sync) && sync == 800);
    assert(!mulib_drm_syncobj_wait(fd, &sync, 1, INT64_MAX, 0));
    assert(mulib_drm_syncobj_wait(fd, NULL, 1, 0, 0) == -1 && errno == EINVAL);
    inject(0, DRM_IOCTL_SYNCOBJ_WAIT, 0, ETIME);
    assert(mulib_drm_syncobj_wait(fd, &sync, 1, INT64_MAX, 0) == -1 && errno == ETIME);
    assert(!mulib_drm_syncobj_signal(fd, &sync, 1));
    assert(!mulib_drm_syncobj_destroy(fd, sync));
    assert(mulib_drm_framebuffer_add(fd, UINT32_MAX, 1, DRM_FORMAT_XRGB8888,
                                     10, UINT32_MAX, &fb) == -1 && errno == EOVERFLOW);
    assert(mulib_drm_framebuffer_add(fd, 8, 4, DRM_FORMAT_XRGB8888, 10, 1, &fb) == -1);
    assert(errno == EINVAL && fb == 99);
    inject(0, DRM_IOCTL_SET_MASTER, 0, EACCES);
    assert(mulib_drm_session_master(fd, 1) == -1 && errno == EACCES);
    assert(!mulib_drm_session_master(fd, 0));
    assert(!mulib_drm_session_vt_mode(123, 1) && mock.vt_calls == 2 &&
           mock.vt_value == KD_GRAPHICS);
    assert(!mulib_drm_session_vt_ack(123, 1) && mock.vt_value == VT_ACKACQ);
    assert(!mulib_drm_session_vt_ack(123, 0) && mock.vt_value == 1);
}

static void count_flip(void *u, uint64_t cookie, uint32_t seq, uint32_t sec,
                       uint32_t usec)
{
    int *count = u;
    assert(cookie == UINT64_MAX && seq == 12 && sec == 13 && usec == 14);
    ++*count;
}

static struct drm_event_vblank flip_event(uint32_t type, uint64_t cookie)
{
    struct drm_event_vblank v = {0};
    v.base.type = type; v.base.length = sizeof v;
    v.user_data = cookie; v.sequence = 12; v.tv_sec = 13; v.tv_usec = 14;
    return v;
}

static void test_events(void)
{
    struct drm_event_vblank v = flip_event(DRM_EVENT_FLIP_COMPLETE, UINT64_MAX);
    unsigned char unaligned[1 + sizeof v * 2 + 1];
    int count = 0, pipefd[2];
    memcpy(unaligned + 1, &v, sizeof v);
    memcpy(unaligned + 1 + sizeof v, &v, sizeof v);
    assert(mulib_drm_pageflip_parse(unaligned + 1, sizeof v * 2, count_flip, &count) == 2);
    assert(count == 2);
    count = 0;
    assert(mulib_drm_pageflip_parse(unaligned + 1, sizeof v * 2 + 1, count_flip, &count) == -1);
    assert(errno == EPROTO && !count);
    assert(mulib_drm_pageflip_parse(NULL, 1, NULL, NULL) == -1 && errno == EINVAL);
    assert(!mulib_drm_pageflip_parse(NULL, 0, NULL, NULL));
    v.base.length = 0;
    assert(mulib_drm_pageflip_parse(&v, sizeof v, NULL, NULL) == -1 && errno == EPROTO);
    v = flip_event(DRM_EVENT_VBLANK, UINT64_MAX);
    assert(!mulib_drm_pageflip_parse(&v, sizeof v, count_flip, &count) && !count);
    v = flip_event(DRM_EVENT_FLIP_COMPLETE, UINT64_MAX);
    assert(!pipe(pipefd));
    assert(write(pipefd[1], &v, sizeof v) == (ssize_t)sizeof v);
    assert(mulib_drm_pageflip_read(pipefd[0], count_flip, &count) == 1 && count == 1);
    assert(!close(pipefd[1]));
    assert(mulib_drm_pageflip_read(pipefd[0], NULL, NULL) == -1 && errno == EPIPE);
    assert(!close(pipefd[0]));
}

static void test_backend(int fd)
{
    mulib_drm_device_t device = {fd};
    mulib_glx_backend_drm_t b = {0};
    struct drm_mode_modeinfo mode = test_mode();
    struct drm_event_vblank event;
    mgx_surface source = {0};
    reset_mock();
    assert(mulib_glx_backend_drm_init(&b, &device, 100, 41, &mode) == 0);
    assert(errno == EINVAL && !b.device && !mock.creates);
    mock.route[0] = 77;
    assert(mulib_glx_backend_drm_init(&b, &device, 100, 9001, &mode) == 0);
    assert(errno == EBUSY && !b.device && !mock.creates);
    mock.route[0] = 9001;
    assert(mulib_glx_backend_drm_init(&b, &device, 100, 9001, &mode));
    assert(b.active && b.old_connector_count == 2 && mock.live_handles == 2 &&
           mock.live_fbs == 2 && mock.route[1] == 0 && mock.sets == 1);
    assert(!mulib_glx_backend_drm_init(&b, &device, 100, 9001, &mode) && errno == EBUSY);
    assert(mgx_surface_init(&source, 8, 4));
    mgx_clear(&source, 0xffabcdef);
    inject(0, DRM_IOCTL_MODE_PAGE_FLIP, 0, EBUSY);
    assert(!mulib_glx_backend_drm_present(&b, &source, NULL, 0) && errno == EBUSY);
    assert(!b.pending && !b.front);
    assert(mulib_glx_backend_drm_present(&b, &source, NULL, 0) && b.pending);
    assert(b.maps[1].pixels[0] == 0xffabcdef);
    assert(!mulib_glx_backend_drm_present(&b, &source, NULL, 0) && errno == EBUSY);
    assert(mulib_glx_backend_drm_release_checked(&b) == -1 && errno == EBUSY);
    assert(mock.sets == 1 && mock.live_fbs == 2);
    event = flip_event(DRM_EVENT_VBLANK, (uintptr_t)&b);
    assert(!mulib_drm_pageflip_parse(&event, sizeof event, mulib_glx_backend_drm_flip_done, &b));
    assert(b.pending && !b.front);
    event = flip_event(DRM_EVENT_FLIP_COMPLETE, 123);
    assert(mulib_drm_pageflip_parse(&event, sizeof event, mulib_glx_backend_drm_flip_done, &b) == 1);
    assert(b.pending);
    event.user_data = (uintptr_t)&b;
    assert(mulib_drm_pageflip_parse(&event, sizeof event, mulib_glx_backend_drm_flip_done, &b) == 1);
    assert(!b.pending && b.front == 1);
    inject(0, DRM_IOCTL_MODE_SETCRTC, 0, EACCES);
    assert(mulib_glx_backend_drm_release_checked(&b) == -1 && errno == EACCES);
    assert(b.active && mock.live_fbs == 2 && mock.live_handles == 2);
    inject(0, DRM_IOCTL_MODE_RMFB, 0, EBUSY);
    assert(mulib_glx_backend_drm_release_checked(&b) == -1 && errno == EBUSY);
    assert(!b.active && b.fb[0] && mock.live_handles == 2);
    assert(mock.last_n == 2 && mock.last_connectors[0] == 100 &&
           mock.last_connectors[1] == 101 && mock.last_x == 3 && mock.last_y == 7);
    assert(mock.route[0] == 9001 && mock.route[1] == 9001);
    inject(0, DRM_IOCTL_MODE_DESTROY_DUMB, 0, EBUSY);
    assert(mulib_glx_backend_drm_release_checked(&b) == -1 && errno == EBUSY);
    assert(!b.fb[0] && b.buffers[0].fd >= 0 && mock.live_handles == 2);
    assert(!mulib_glx_backend_drm_release_checked(&b) && !b.device);
    assert(!mock.live_handles && !mock.live_fbs && mock.sets == 2);
    assert(!mulib_glx_backend_drm_release_checked(&b));
    mgx_surface_free(&source);

    /* Partial init rolls back; failed rollback retains a retryable owner. */
    reset_mock();
    inject(0, DRM_IOCTL_MODE_ADDFB2, 1, ENOSPC);
    inject(1, DRM_IOCTL_MODE_RMFB, 0, EBUSY);
    assert(!mulib_glx_backend_drm_init(&b, &device, 100, 9001, &mode));
    assert(errno == ENOSPC && b.device && !b.active && b.fb[0]);
    assert(mock.live_handles == 2 && mock.live_fbs == 1 && !mock.sets);
    assert(!mulib_glx_backend_drm_init(&b, &device, 100, 9001, &mode) && errno == EBUSY);
    assert(!mulib_glx_backend_drm_release_checked(&b));
    assert(!b.device && !mock.live_handles && !mock.live_fbs);
}

static void test_edid(void)
{
    unsigned char edid[256] = {0,255,255,255,255,255,255,0};
    edid[126] = 1;
    edid[127] = 5; /* header sum (6*255 + extension count + checksum) % 256 == 0 */
    assert(mulib_drm_output_edid_valid(edid, sizeof edid));
    assert(!mulib_drm_output_edid_valid(edid, 128));
    edid[200] = 1;
    assert(!mulib_drm_output_edid_valid(edid, sizeof edid));
}

int main(void)
{
    char path[64];
    int fd = temporary_file(path);
    assert(mulib_drm_kms_supported() && MULIB_GFX_DRM_BACKEND);
    test_native(fd, path);
    test_enumeration(fd);
    test_atomic(fd);
    test_handles(fd);
    test_events();
    test_backend(fd);
    test_edid();
    assert(!close(fd) && !unlink(path));
    puts("gfx-drm: UAPI ABI and mock tests passed (no DRM devices accessed)");
    return 0;
}
#else
static int mock_ioctl(int fd, unsigned long op, void *arg)
{ (void)fd; (void)op; (void)arg; assert(!"unexpected ioctl"); return -1; }
static int mock_vt_ioctl(int fd, unsigned long op, int value)
{ (void)fd; (void)op; (void)value; assert(!"unexpected VT ioctl"); return -1; }
int main(void)
{
    uint32_t id = 99;
    assert(!mulib_drm_kms_supported() && !MULIB_GFX_DRM_BACKEND);
    assert(mulib_drm_kms_ioctl(123, 0, NULL) == -1 && errno == ENOTSUP);
    assert(mulib_drm_syncobj_create(123, 0, &id) == -1 && errno == ENOTSUP && id == 99);
    assert(mulib_drm_session_master(123, 1) == -1 && errno == ENOTSUP);
    puts("gfx-drm: explicit no-UAPI capability tests passed");
    return 0;
}
#endif
