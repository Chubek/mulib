/* A private socketpair/local-listener compositor fixture. No display/devices. */
#include <stdlib.h>
#include <errno.h>

static int fail_after = -1;
static void *fixture_realloc(void *p, size_t n)
{
    if (fail_after >= 0 && fail_after-- == 0) {
        errno = ENOMEM;
        return NULL;
    }
    return realloc(p, n);
}
#define realloc fixture_realloc
#include "mulib-glx-backend-wayland.h"
#include "mulib-glx-present.h"
#include "mulib-glx-window.h"
#undef realloc
#include <assert.h>
#include <dirent.h>

typedef struct {
    int fd[2];
    mulib_glx_backend_wayland_client_t client;
    mway_stream server;
} fixture;

typedef struct {
    int fd;
    unsigned char *pixels;
    size_t bytes, words[2];
    int offsets[2];
    mgx_surface maps[2];
} fixture_pool;

static size_t fd_count(void)
{
    DIR *dir = opendir("/proc/self/fd");
    assert(dir);
    size_t n = 0;
    struct dirent *e;
    while ((e = readdir(dir)))
        if (strcmp(e->d_name, ".") && strcmp(e->d_name, ".."))
            n++;
    assert(!closedir(dir));
    return n;
}

static void fixture_open(fixture *f)
{
    memset(f, 0, sizeof *f);
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, f->fd));
    int small = 1024;
    assert(!setsockopt(f->fd[0], SOL_SOCKET, SO_SNDBUF, &small, sizeof small));
    assert(mulib_glx_backend_wayland_client_init(
        &f->client, f->fd[0], "MuLIB fixture", 2, 2, 1048576));
    assert(fcntl(f->fd[0], F_GETFD) & FD_CLOEXEC);
}

static void fixture_close(fixture *f)
{
    int fd = f->fd[0];
    mulib_glx_backend_wayland_client_free(&f->client);
    assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
    mulib_wayland_wire_stream_free(&f->server);
    assert(!close(f->fd[1]));
}

static void send_bytes(int fd, const void *bytes, size_t n)
{
    assert(send(fd, bytes, n, MSG_NOSIGNAL) == (ssize_t)n);
}

static void server_event(fixture *f, uint32_t object, uint16_t opcode,
                         const void *args, size_t n)
{
    mway_bytes bytes = {0};
    assert(mway_message(&bytes, object, opcode, args, n));
    send_bytes(f->fd[1], bytes.v, bytes.n);
    mway_bytes_free(&bytes);
}

static void server_global(mway_bytes *bytes, uint32_t name, const char *interface)
{
    mway_bytes args = {0};
    assert(mway_u32(&args, name) && mway_string(&args, interface) &&
           mway_u32(&args, 99) &&
           mway_message(bytes, 2, 0, args.v, args.n));
    mway_bytes_free(&args);
}

static mway_frame server_peek(fixture *f, uint32_t object, uint16_t opcode,
                              size_t n)
{
    mway_frame frame;
    for (unsigned i = 0;; i++) {
        assert(i < 100);
        int r = mulib_wayland_wire_stream_peek(&f->server, &frame);
        assert(r >= 0);
        if (r == 1)
            break;
        assert(mulib_wayland_wire_stream_receive(&f->server, f->fd[1]) > 0);
    }
    if (frame.object != object || frame.opcode != opcode || frame.n != n)
        fprintf(stderr, "frame got %u/%u/%zu expected %u/%u/%zu\n",
                frame.object, frame.opcode, frame.n, object, opcode, n);
    assert(frame.object == object && frame.opcode == opcode && frame.n == n);
    return frame;
}

static void server_consume(fixture *f)
{
    assert(mulib_wayland_wire_stream_consume(&f->server, 0, NULL) == 1);
}

static void server_expect(fixture *f, uint32_t object, uint16_t opcode, size_t n)
{
    (void)server_peek(f, object, opcode, n);
    server_consume(f);
}

static int receive_dispatch(fixture *f)
{
    assert(mulib_glx_backend_wayland_client_receive(&f->client) == 1);
    return mulib_glx_backend_wayland_client_dispatch(&f->client, 1024);
}

static void bootstrap(fixture *f)
{
    fixture_open(f);
    assert(mulib_glx_backend_wayland_client_receive(&f->client) == 0);
    assert(mulib_glx_backend_wayland_client_flush(&f->client) == 1);
    mway_frame frame = server_peek(f, 1, 1, 4);
    assert(mway_client_u32(frame.args) == 2);
    server_consume(f);
    frame = server_peek(f, 1, 0, 4);
    assert(mway_client_u32(frame.args) == 3);
    server_consume(f);
    mway_bytes bytes = {0};
    server_global(&bytes, 10, "wl_compositor");
    server_global(&bytes, 20, "wl_shm");
    server_global(&bytes, 30, "xdg_wm_base");
    server_global(&bytes, 40, "unused_fixture_interface");
    uint32_t serial = 77, sync = 3;
    assert(mway_message(&bytes, 3, 0, &serial, 4) &&
           mway_message(&bytes, 1, 1, &sync, 4));
    send_bytes(f->fd[1], bytes.v, 3);
    assert(receive_dispatch(f) == 0); /* Retain partial header. */
    send_bytes(f->fd[1], bytes.v + 3, bytes.n - 3);
    assert(mulib_glx_backend_wayland_client_receive(&f->client) == 1);
    for (size_t i = 0; i < 6; i++)
        assert(mulib_glx_backend_wayland_client_dispatch(&f->client, 1) == 1);
    assert(mulib_glx_backend_wayland_client_dispatch(&f->client, 1) == 0);
    mway_bytes_free(&bytes);
    assert(f->client.state == MWAY_CLIENT_CONFIGURE);
    assert(!f->client.backend.configured && !f->client.retired_n);
    assert(mulib_glx_backend_wayland_client_flush(&f->client) == 1);
    const char *names[] = {"wl_compositor", "wl_shm", "xdg_wm_base"};
    for (size_t i = 0; i < 3; i++) {
        size_t padded = (strlen(names[i]) + 4) & ~(size_t)3;
        frame = server_peek(f, 2, 0, 16 + padded);
        assert(mway_client_u32(frame.args) == (i + 1) * 10);
        assert(!strcmp((const char *)frame.args + 8, names[i]));
        assert(mway_client_u32(frame.args + 8 + padded) == 1);
        assert(mway_client_u32(frame.args + 12 + padded) == 4 + i);
        server_consume(f);
    }
    server_expect(f, 4, 0, 4);
    server_expect(f, 6, 2, 8);
    server_expect(f, 8, 1, 4);
    server_expect(f, 9, 2, 20);
    server_expect(f, 7, 6, 0);
    uint32_t format = 0, ping = 123, top[] = {2, 2, 0};
    server_event(f, 5, 0, &format, 4);
    server_event(f, 6, 0, &ping, 4);
    server_event(f, 9, 0, top, sizeof top);
    server_event(f, 8, 0, &serial, 4);
    assert(receive_dispatch(f) == 4);
    assert(f->client.state == MWAY_CLIENT_READY && f->client.resize_pending);
    assert(mulib_glx_backend_wayland_client_flush(&f->client) == 1);
    frame = server_peek(f, 6, 3, 4);
    assert(mway_client_u32(frame.args) == ping);
    server_consume(f);
    frame = server_peek(f, 8, 4, 4);
    assert(mway_client_u32(frame.args) == serial);
    server_consume(f);
}

static void pool_open(fixture_pool *p, int w, int h)
{
    memset(p, 0, sizeof *p);
    char path[] = "/tmp/opencode/mulib-wayland-fixture-XXXXXX";
    p->fd = mkstemp(path);
    assert(p->fd >= 0 && !unlink(path));
    size_t stride = (size_t)w + 1;
    p->words[0] = p->words[1] = stride * (size_t)h;
    p->bytes = p->words[0] * 8;
    p->offsets[1] = (int)(p->bytes / 2);
    assert(!ftruncate(p->fd, (off_t)p->bytes));
    p->pixels = mmap(NULL, p->bytes, PROT_READ | PROT_WRITE,
                     MAP_SHARED, p->fd, 0);
    assert(p->pixels != MAP_FAILED);
    for (size_t i = 0; i < 2; i++)
        assert(mgx_wrap_capacity(&p->maps[i],
            (uint32_t *)(p->pixels + p->offsets[i]), p->words[i], stride, w, h));
}

static void pool_close(fixture_pool *p)
{
    if (p->fd >= 0)
        assert(!close(p->fd));
    assert(!munmap(p->pixels, p->bytes));
}

static void pool_install(fixture *f, fixture_pool *p, int replacement)
{
    uint32_t old[] = {f->client.backend.buffer[0], f->client.backend.buffer[1]};
    assert(mulib_glx_backend_wayland_client_buffers(
        &f->client, p->fd, p->bytes, p->offsets, p->maps, p->words));
    uint32_t pool = f->client.next_id - 2;
    assert(f->client.control.has_fd);
    assert(close(p->fd) == 0);
    p->fd = -1; /* TX owns a duplicate, never the original FD. */
    /* Ping while the FD-bearing control batch is queued must be appendable. */
    uint32_t ping = 55;
    server_event(f, 6, 0, &ping, 4);
    assert(receive_dispatch(f) == 1);
    assert(mulib_glx_backend_wayland_client_flush(&f->client) == 1);
    if (replacement)
        for (size_t i = 0; i < 2; i++)
            server_expect(f, old[i], 0, 0);
    mway_frame frame = server_peek(f, 5, 0, 8);
    assert(mway_client_u32(frame.args) == pool);
    assert(mway_client_u32(frame.args + 4) == p->bytes);
    int received = -1;
    assert(mulib_wayland_wire_stream_consume(&f->server, 1, &received) == 1);
    assert(received >= 0 && (fcntl(received, F_GETFD) & FD_CLOEXEC));
    struct stat st;
    assert(!fstat(received, &st) && (size_t)st.st_size == p->bytes);
    assert(!close(received));
    for (size_t i = 0; i < 2; i++) {
        frame = server_peek(f, pool, 0, 24);
        assert(mway_client_u32(frame.args) == f->client.backend.buffer[i]);
        assert(mway_client_u32(frame.args + 4) == (uint32_t)p->offsets[i]);
        assert(mway_client_u32(frame.args + 16) == p->maps[i].stride * 4);
        server_consume(f);
    }
    server_expect(f, pool, 1, 0);
    server_expect(f, 6, 3, 4);
    if (replacement)
        for (size_t i = 0; i < 2; i++)
            server_event(f, 1, 1, old + i, 4);
    server_event(f, 1, 1, &pool, 4);
    assert(receive_dispatch(f) == (replacement ? 3 : 1));
}

typedef struct {
    mulib_glx_backend_wayland_client_t *client;
    mulib_glx_present_t *present;
} present_host;

static int host_present(void *u, const mgx_surface *source,
                         const mgx_rect *damage, size_t n)
{
    present_host *host = u;
    return mulib_glx_backend_wayland_client_begin(
        host->client, source, damage, n, host->present->serial + 1);
}

static uint32_t submit(fixture *f, mgx_surface *source,
                       mulib_glx_present_t *present)
{
    present_host host = {&f->client, present};
    mulib_glx_platform_t platform = {
        .user = &host, .ops = {.present = host_present},
        .capabilities = MGX_CAP_WINDOWS | MGX_CAP_ASYNC_PRESENT};
    assert(mulib_glx_present_submit(present, &platform, source));
    assert(present->pending);
    uint32_t frame = f->client.frame_id;
    assert(mulib_glx_backend_wayland_client_flush(&f->client) == 1);
    server_expect(f, 7, 1, 12);
    server_expect(f, 7, 2, 16);
    mway_frame wire = server_peek(f, 7, 3, 4);
    assert(mway_client_u32(wire.args) == frame);
    server_consume(f);
    server_expect(f, 7, 6, 0);
    return frame;
}

static void complete_frame(fixture *f, uint32_t frame,
                            mulib_glx_present_t *present)
{
    uint32_t milliseconds = 9001;
    server_event(f, frame, 0, &milliseconds, 4);
    server_event(f, 1, 1, &frame, 4);
    assert(receive_dispatch(f) == 2);
    uint64_t token = 0;
    uint32_t time = 0;
    assert(mulib_glx_backend_wayland_client_take_frame(
        &f->client, &token, &time));
    assert(token == present->serial && time == milliseconds);
    assert(mulib_glx_present_complete(present, token));
    assert(!mulib_glx_backend_wayland_client_take_frame(
        &f->client, &token, &time));
}

static void test_lifecycle(void)
{
    fixture f;
    bootstrap(&f);
    fixture_pool old, next;
    pool_open(&old, 2, 2);
    pool_install(&f, &old, 0);
    uint32_t pixels[] = {0x80ff8040, 0xff123456, 0, 0xffffffff};
    mgx_surface source = {0};
    assert(mgx_wrap_capacity(&source, pixels, 4, 2, 2, 2));
    mulib_glx_present_t present = {0};
    uint32_t frame = submit(&f, &source, &present);
    assert(old.maps[0].pixels[0] == 0x80804020);
    pixels[0] = 0;
    assert(old.maps[0].pixels[0] == 0x80804020);
    complete_frame(&f, frame, &present);
    assert(f.client.backend.busy[0]); /* Frame done does not release storage. */
    frame = submit(&f, &source, &present);
    uint32_t top[] = {3, 1, 0}, serial = 78;
    server_event(&f, 9, 0, top, sizeof top);
    server_event(&f, 8, 0, &serial, 4);
    assert(receive_dispatch(&f) == 2);
    assert(f.client.resize_pending && !f.client.backend.configured);
    assert(mulib_glx_backend_wayland_client_flush(&f.client) == 1);
    server_expect(&f, 8, 4, 4);
    pool_open(&next, 3, 1);
    assert(!mulib_glx_backend_wayland_client_buffers(
        &f.client, next.fd, next.bytes, next.offsets, next.maps, next.words));
    assert(errno == EBUSY);
    server_event(&f, f.client.backend.buffer[1], 0, NULL, 0);
    assert(receive_dispatch(&f) == 1);
    assert(!f.client.backend.busy[1] && present.pending);
    complete_frame(&f, frame, &present);
    assert(!mulib_glx_backend_wayland_client_buffers(
        &f.client, next.fd, next.bytes, next.offsets, next.maps, next.words));
    assert(errno == EBUSY); /* Other old buffer is still compositor-owned. */
    server_event(&f, f.client.backend.buffer[0], 0, NULL, 0);
    assert(receive_dispatch(&f) == 1);
    pool_install(&f, &next, 1);
    pool_close(&old); /* Replacement succeeded; old maps may now disappear. */
    server_event(&f, 9, 1, NULL, 0);
    assert(receive_dispatch(&f) == 1 && f.client.close_requested);
    assert(!mulib_glx_backend_wayland_client_begin(
        &f.client, &source, NULL, 0, 3));
    assert(mulib_glx_present_try_free(&present));
    fixture_close(&f);
    pool_close(&next);
}

static void test_failures(void)
{
    for (unsigned which = 0; which < 10; which++) {
        fixture f;
        bootstrap(&f);
        uint32_t arg = 0, top[] = {2, 2, 0};
        if (which == 0)
            server_event(&f, 99, 0, NULL, 0);
        else if (which == 1)
            server_event(&f, 1, 1, &arg, 4); /* Unknown delete_id. */
        else if (which == 2)
            server_event(&f, 9, 0, top, 8); /* Missing states array. */
        else if (which == 3) {
            top[0] = UINT32_MAX;
            server_event(&f, 9, 0, top, sizeof top);
        } else if (which == 4) {
            arg = 10;
            server_event(&f, 2, 1, &arg, 4); /* Required global removed. */
        } else if (which == 5)
            server_event(&f, 6, 0, NULL, 0); /* Wrong ping arity. */
        else if (which == 6) {
            uint32_t partial[] = {8, 12u << 16};
            send_bytes(f.fd[1], partial, 7);
            assert(!shutdown(f.fd[1], SHUT_WR));
        } else if (which == 7) {
            mway_tx tx = {0};
            int pipefd[2];
            assert(!pipe(pipefd));
            assert(mulib_wayland_wire_tx_begin(
                &tx, 6, 0, &arg, 4, pipefd[0]));
            assert(mulib_wayland_wire_tx_step(&tx, f.fd[1]) == 1);
            mulib_wayland_wire_tx_free(&tx);
            assert(!close(pipefd[0]) && !close(pipefd[1]));
        } else if (which == 8) {
            uint32_t malformed[] = {0, 8u << 16};
            send_bytes(f.fd[1], malformed, sizeof malformed);
        } else {
            arg = 0x70000000;
            server_event(&f, 6, 0, &arg, 4);
            fail_after = 0; /* Failed pong allocation aborts/cleans the client. */
        }
        int r = mulib_glx_backend_wayland_client_receive(&f.client);
        if (r >= 0) {
            r = mulib_glx_backend_wayland_client_dispatch(&f.client, 1024);
            if (which == 6 && r >= 0) {
                assert(mulib_glx_backend_wayland_client_receive(&f.client) == 1);
                r = mulib_glx_backend_wayland_client_dispatch(&f.client, 1024);
            }
        }
        fail_after = -1;
        assert(r == -1 && f.client.failed && f.client.backend.fd == -1);
        fixture_close(&f);
    }
}

static void test_validation_and_allocations(void)
{
    fixture f;
    bootstrap(&f);
    fixture_pool pool;
    pool_open(&pool, 2, 2);
    size_t base = fd_count();
    size_t saved = pool.words[0];
    pool.words[0]--;
    assert(!mulib_glx_backend_wayland_client_buffers(
        &f.client, pool.fd, pool.bytes, pool.offsets, pool.maps, pool.words));
    pool.words[0] = saved;
    int saved_offset = pool.offsets[1];
    pool.offsets[1] = 0;
    assert(!mulib_glx_backend_wayland_client_buffers(
        &f.client, pool.fd, pool.bytes, pool.offsets, pool.maps, pool.words));
    pool.offsets[1] = saved_offset;
    assert(!mulib_glx_backend_wayland_client_buffers(
        &f.client, pool.fd, pool.bytes - 1, pool.offsets, pool.maps, pool.words));
    for (int attempt = 0; attempt < 4; attempt++) {
        fail_after = attempt;
        int r = mulib_glx_backend_wayland_client_buffers(
            &f.client, pool.fd, pool.bytes, pool.offsets, pool.maps, pool.words);
        fail_after = -1;
        assert(!r && !f.client.backend.buffer[0] && !f.client.control.bytes.n);
        assert(f.client.next_id == 9 && fd_count() == base);
    }
    pool_install(&f, &pool, 0);
    uint32_t pixels[4] = {0};
    mgx_surface source = {0};
    assert(mgx_wrap_capacity(&source, pixels, 4, 2, 2, 2));
    /* Failure at each transport allocation, including after pixel-copy, must
     * not consume the logical token, pin a slot, or leak queued request bytes. */
    unsigned failed = 0;
    for (int attempt = 0; attempt < 12; attempt++) {
        fail_after = attempt;
        int r = mulib_glx_backend_wayland_client_begin(
            &f.client, &source, NULL, 0, 1);
        fail_after = -1;
        if (r) {
            f.client.backend.busy[0] = 0;
            mulib_wayland_wire_tx_free(&f.client.backend.tx);
            f.client.frame_id = 0;
            break;
        }
        failed++;
        assert(!f.client.backend.busy[0] && !f.client.frame_id &&
               !f.client.backend.tx.bytes.n);
    }
    assert(failed >= 4);
    fixture_close(&f);
    pool_close(&pool);
}

static void test_backpressure_and_disconnect(void)
{
    fixture f;
    bootstrap(&f);
    /* Queue enough legal ping responses to force a short nonblocking write. */
    uint32_t serial = 123;
    for (unsigned i = 0; i < 4096; i++)
        assert(mway_client_control(&f.client, 6, 3, &serial, 4) == 1);
    assert(mulib_glx_backend_wayland_client_flush(&f.client) == 0);
    assert(f.client.control.sent > 0 &&
           f.client.control.sent < f.client.control.bytes.n);
    for (unsigned turns = 0; f.client.control.bytes.n; turns++) {
        assert(turns < 10000);
        unsigned char bytes[8192];
        assert(recv(f.fd[1], bytes, sizeof bytes, MSG_DONTWAIT) > 0);
        assert(mulib_glx_backend_wayland_client_flush(&f.client) >= 0);
    }
    /* Drain the final complete responses before checking the failure path. */
    unsigned char bytes[8192];
    while (recv(f.fd[1], bytes, sizeof bytes, MSG_DONTWAIT) > 0) {}
    assert(errno == EAGAIN || errno == EWOULDBLOCK);
    assert(!shutdown(f.fd[1], SHUT_RDWR));
    assert(mway_client_control(&f.client, 6, 3, &serial, 4) == 1);
    assert(mulib_glx_backend_wayland_client_flush(&f.client) == -1);
    assert(f.client.failed && !f.client.control.bytes.v);
    fixture_close(&f);
}

static void test_presentation_control_order(void)
{
    fixture f;
    bootstrap(&f);
    fixture_pool pool;
    pool_open(&pool, 2, 2);
    pool_install(&f, &pool, 0);
    uint32_t pixels[4] = {0};
    mgx_surface source = {0};
    assert(mgx_wrap_capacity(&source, pixels, 4, 2, 2, 2));
    mgx_rect damage[4096];
    for (size_t i = 0; i < 4096; i++)
        damage[i] = (mgx_rect){0, 0, 1, 1};
    assert(mulib_glx_backend_wayland_client_begin(
        &f.client, &source, damage, 4096, 10));
    /* A configure received after queueing, but before the first frame byte,
     * must not cause its ack to overtake that older frame's commit. */
    uint32_t top[] = {2, 2, 0}, configure = 80;
    server_event(&f, 9, 0, top, sizeof top);
    server_event(&f, 8, 0, &configure, 4);
    assert(receive_dispatch(&f) == 2 && f.client.control.bytes.n == 12);
    assert(mulib_glx_backend_wayland_client_flush(&f.client) == 0);
    assert(f.client.backend.tx.sent && f.client.backend.tx.bytes.n);
    uint32_t ping = 88;
    server_event(&f, 6, 0, &ping, 4);
    assert(receive_dispatch(&f) == 1 && f.client.control.bytes.n == 24);
    mway_bytes wire = {0};
    for (unsigned turns = 0;; turns++) {
        assert(turns < 10000);
        unsigned char bytes[4096];
        ssize_t n = recv(f.fd[1], bytes, sizeof bytes, MSG_DONTWAIT);
        if (n > 0)
            assert(mway_append(&wire, bytes, (size_t)n));
        else
            assert(n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));
        int r = mulib_glx_backend_wayland_client_flush(&f.client);
        assert(r >= 0);
        if (r == 1 && wire.n == 20 + 4096 * 24 + 12 + 8 + 12 + 12)
            break;
    }
    size_t at = 0;
    for (unsigned i = 0; i < 4101; i++) {
        mway_frame frame;
        assert(mulib_wayland_wire_parse(wire.v + at, wire.n - at, &frame) == 1);
        if (!i)
            assert(frame.object == 7 && frame.opcode == 1);
        else if (i <= 4096)
            assert(frame.object == 7 && frame.opcode == 2);
        else if (i == 4097)
            assert(frame.object == 7 && frame.opcode == 3);
        else if (i == 4098)
            assert(frame.object == 7 && frame.opcode == 6);
        else if (i == 4099)
            assert(frame.object == 8 && frame.opcode == 4 &&
                   mway_client_u32(frame.args) == configure);
        else
            assert(frame.object == 6 && frame.opcode == 3);
        at += frame.consumed;
    }
    assert(at == wire.n);
    mway_bytes_free(&wire);
    /* Disconnect is necessary before abandoning an accepted frame. */
    mulib_glx_present_t present = {.serial = 10, .pending = 1};
    assert(!mulib_glx_present_abort(&present, 9));
    fixture_close(&f);
    assert(mulib_glx_present_abort(&present, 10));
    assert(!present.presented && present.serial == 10);
    assert(!mulib_glx_present_abort(&present, 10));
    assert(mulib_glx_present_try_free(&present));
    pool_close(&pool);
}

static int retain_mock(void *u, const mgx_surface *source,
                        const mgx_rect *damage, size_t n)
{
    (void)u;
    (void)source;
    (void)damage;
    (void)n;
    return 1;
}

static void test_window_abort(void)
{
    mulib_glx_platform_t platform = {
        .ops = {.present = retain_mock}, .capabilities = MGX_CAP_ASYNC_PRESENT};
    mulib_glx_window_t window = {0};
    assert(mulib_glx_window_init(&window, &platform, 1, "abort", 2, 2));
    assert(mulib_glx_swapchain_acquire(&window.swap));
    assert(mulib_glx_window_present(&window));
    assert(!mulib_glx_window_try_free(&window));
    assert(!mulib_glx_window_abort(&window, 2));
    /* Mock owns no references after this explicit quiescence point. */
    assert(mulib_glx_window_abort(&window, 1));
    assert(!window.present.presented && window.present.serial == 1);
    assert(mulib_glx_window_try_free(&window));
}

static void test_private_connect(void)
{
    char path[] = "/tmp/opencode/mulib-wayland-socket-XXXXXX";
    int tmp = mkstemp(path);
    assert(tmp >= 0 && !close(tmp) && !unlink(path));
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    assert(strlen(path) < sizeof addr.sun_path);
    strcpy(addr.sun_path, path);
    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    assert(listener >= 0);
    assert(!bind(listener, (const struct sockaddr *)&addr, sizeof addr));
    assert(!listen(listener, 1));
    mulib_glx_backend_wayland_client_t c = {0};
    assert(mulib_glx_backend_wayland_client_connect(
        &c, path, "private", 2, 2, 4096));
    int peer = accept(listener, NULL, NULL);
    assert(peer >= 0);
    assert(mulib_glx_backend_wayland_client_flush(&c) == 1);
    unsigned char bytes[24];
    assert(recv(peer, bytes, sizeof bytes, MSG_WAITALL) == 24);
    mulib_glx_backend_wayland_client_free(&c);
    assert(!close(peer) && !close(listener) && !unlink(path));
}

static void test_ownership_and_quotas(void)
{
    int fd[2];
    assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, fd));
    mulib_glx_backend_wayland_client_t c = {0};
    fail_after = 1; /* Partial get_registry allocation must be freed. */
    assert(!mulib_glx_backend_wayland_client_init(
        &c, fd[0], "failure", 2, 2, 4096));
    fail_after = -1;
    assert(!c.active && fcntl(fd[0], F_GETFD) >= 0);
    assert(!close(fd[0]) && !close(fd[1]));

    fixture f;
    bootstrap(&f);
    fixture_pool pool;
    pool_open(&pool, 2, 2);
    pool_install(&f, &pool, 0);
    uint32_t pixels[4] = {0};
    mgx_surface source = {0};
    assert(mgx_wrap_capacity(&source, pixels, 4, 2, 2, 2));
    uint32_t oldest = 0;
    for (unsigned i = 0; i < 64; i++) {
        assert(mulib_glx_backend_wayland_client_begin(
            &f.client, &source, NULL, 0, (uint64_t)i + 1));
        uint32_t callback = f.client.frame_id, time = i;
        if (!i)
            oldest = callback;
        assert(mulib_glx_backend_wayland_client_flush(&f.client) == 1);
        server_expect(&f, 7, 1, 12);
        server_expect(&f, 7, 2, 16);
        server_expect(&f, 7, 3, 4);
        server_expect(&f, 7, 6, 0);
        /* Release before frame done must not complete the host's frame. */
        server_event(&f, f.client.backend.buffer[0], 0, NULL, 0);
        assert(receive_dispatch(&f) == 1 && f.client.frame_id);
        server_event(&f, callback, 0, &time, 4);
        assert(receive_dispatch(&f) == 1);
        uint64_t token;
        assert(mulib_glx_backend_wayland_client_take_frame(
            &f.client, &token, &time) && token == (uint64_t)i + 1);
        /* Deliberately withhold delete_id; this must not grow without bound. */
    }
    assert(f.client.retired_n == 64);
    assert(!mulib_glx_backend_wayland_client_begin(
        &f.client, &source, NULL, 0, 65) && errno == EAGAIN);
    server_event(&f, 1, 1, &oldest, 4);
    assert(receive_dispatch(&f) == 1 && f.client.retired_n == 63);
    assert(mulib_glx_backend_wayland_client_begin(
        &f.client, &source, NULL, 0, 65));
    /* Premature release of an unsent commit is fatal; disconnect frees TX. */
    server_event(&f, f.client.backend.buffer[0], 0, NULL, 0);
    assert(receive_dispatch(&f) == -1 && f.client.failed);
    fixture_close(&f);
    pool_close(&pool);

    bootstrap(&f);
    pool_open(&pool, 2, 2);
    pool_install(&f, &pool, 0);
    server_event(&f, f.client.backend.buffer[0], 0, NULL, 0);
    assert(receive_dispatch(&f) == -1); /* Duplicate/idle release. */
    fixture_close(&f);
    pool_close(&pool);
}

int main(void)
{
    size_t base = fd_count();
    test_lifecycle();
    test_failures();
    test_validation_and_allocations();
    test_backpressure_and_disconnect();
    test_presentation_control_order();
    test_window_abort();
    test_private_connect();
    test_ownership_and_quotas();
    assert(fd_count() == base);
    puts("gfx-backend-followup: private Wayland client lifecycle passed");
    return 0;
}
