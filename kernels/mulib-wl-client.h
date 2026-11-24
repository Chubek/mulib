/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT */
#ifndef MULIB_WL_CLIENT_H
#define MULIB_WL_CLIENT_H
#include "mulib-wl-resource.h"
#include "mulib-wayland-protocol.h"
#define MULIB_WL_RX_LIMIT (256u * 1024u)
#define MULIB_WL_FD_LIMIT MULIB_WAYLAND_WIRE_RECEIVE_FDS

/* Zero limits mean a deliberately unusable account, not unlimited resources.
 * A global account is borrowed and must outlive clients AND retained snapshots. */
typedef struct {
    size_t pools, pool_bytes, buffers, surfaces, snapshot_bytes;
} mulib_wl_usage;
typedef struct {
    mulib_wl_usage used, limit;
} mulib_wl_quota;
typedef struct {
    mulib_wl_quota local, *global;
    size_t refs;
} mulib_wl_account;
typedef struct mulib_wl_tx_entry {
    mway_tx wire;
    struct mulib_wl_tx_entry *next;
} mulib_wl_tx_entry;
typedef struct mulib_wl_client {
    int fd, failed, eof, dispatching;
    mulib_wl_resources objects;
    unsigned char *rx, *tx; /* tx/tn/tc/ts retained for source compatibility. */
    size_t rn, rc, tn, tc, ts;
    int fds[MULIB_WL_FD_LIMIT];
    size_t nfds, request_fds;
    void *user;
    mulib_wl_tx_entry *out, *tail;
    mulib_wl_account *account;
} mulib_wl_client;
#define MULIB_WL_CLIENT_INIT { .fd = -1 }

static inline int mulib_wl_usage_fits(mulib_wl_usage used,
        mulib_wl_usage add, mulib_wl_usage limit) {
#define MULIB_WL_FITS(field) \
    if (used.field > limit.field || add.field > limit.field - used.field) return 0
    MULIB_WL_FITS(pools);
    MULIB_WL_FITS(pool_bytes);
    MULIB_WL_FITS(buffers);
    MULIB_WL_FITS(surfaces);
    MULIB_WL_FITS(snapshot_bytes);
#undef MULIB_WL_FITS
    return 1;
}
static inline void mulib_wl_usage_add(mulib_wl_usage *u, mulib_wl_usage v) {
    u->pools += v.pools; u->pool_bytes += v.pool_bytes;
    u->buffers += v.buffers; u->surfaces += v.surfaces;
    u->snapshot_bytes += v.snapshot_bytes;
}
static inline void mulib_wl_usage_sub(mulib_wl_usage *u, mulib_wl_usage v) {
    u->pools -= v.pools; u->pool_bytes -= v.pool_bytes;
    u->buffers -= v.buffers; u->surfaces -= v.surfaces;
    u->snapshot_bytes -= v.snapshot_bytes;
}
static inline int mulib_wl_account_charge(mulib_wl_account *a, mulib_wl_usage v) {
    if (!a || a->refs == SIZE_MAX ||
        !mulib_wl_usage_fits(a->local.used, v, a->local.limit) ||
        (a->global && !mulib_wl_usage_fits(a->global->used, v, a->global->limit)))
        return 0;
    mulib_wl_usage_add(&a->local.used, v);
    if (a->global) mulib_wl_usage_add(&a->global->used, v);
    a->refs++;
    return 1;
}
static inline void mulib_wl_account_release(mulib_wl_account *a, mulib_wl_usage v) {
    if (!a) return;
    mulib_wl_usage_sub(&a->local.used, v);
    if (a->global) mulib_wl_usage_sub(&a->global->used, v);
    if (!--a->refs) free(a);
}
/* Set limits before any resources/charges; global is optional for socketpair
 * fixtures, required for a host accepting more than one client. */
static inline int mulib_wl_client_limits(mulib_wl_client *c,
        mulib_wl_usage limits, mulib_wl_quota *global) {
    if (!c || !c->account || c->account->refs != 1) return 0;
    c->account->local.limit = limits;
    c->account->global = global;
    return 1;
}
static inline int mulib_wl_client_init(mulib_wl_client *c, int fd, size_t limit) {
    if (!c || fd < 0 || !limit) return 0;
    mulib_wl_account *a = calloc(1, sizeof *a);
    if (!a) return 0;
    int f = fcntl(fd, F_GETFD), s = fcntl(fd, F_GETFL);
    if (f < 0 || s < 0 || fcntl(fd, F_SETFD, f | FD_CLOEXEC) < 0 ||
        fcntl(fd, F_SETFL, s | O_NONBLOCK) < 0) { free(a); return 0; }
    a->refs = 1;
    a->local.limit = (mulib_wl_usage){16, 128u*1024u*1024u, 256, 128, 64u*1024u*1024u};
    *c = (mulib_wl_client){.fd = fd, .account = a};
    c->objects.limit = limit;
    return 1;
}
static inline void mulib_wl_client_clear_io(mulib_wl_client *c) {
    for (size_t i = 0; i < c->nfds; i++) close(c->fds[i]);
    c->nfds = c->request_fds = 0;
    while (c->out) {
        mulib_wl_tx_entry *next = c->out->next;
        mulib_wayland_wire_tx_free(&c->out->wire);
        free(c->out);
        c->out = next;
    }
    c->tail = NULL; c->tn = c->ts = 0;
}
static inline int mulib_wl_client_fail(mulib_wl_client *c) {
    if (c) { c->failed = 1; mulib_wl_client_clear_io(c); }
    return -1;
}
static inline void mulib_wl_client_free(mulib_wl_client *c) {
    if (!c) return;
    mulib_wl_resources_free(&c->objects);
    mulib_wl_client_clear_io(c);
    if (c->fd >= 0) close(c->fd);
    free(c->rx); free(c->tx);
    mulib_wl_account_release(c->account, (mulib_wl_usage){0});
    memset(c, 0, sizeof *c); c->fd = -1;
}
static inline int mulib_wl_client_take_fd(mulib_wl_client *c) {
    if (!c || !c->nfds || (c->dispatching && !c->request_fds)) return -1;
    int fd = c->fds[0];
    memmove(c->fds, c->fds + 1, (--c->nfds) * sizeof(int));
    if (c->dispatching) c->request_fds--;
    return fd;
}
/* 1 progress, 0 would-block, -1 fatal. EOF is dispatched once so complete
 * buffered requests can be inspected, then dispatch reports a closed peer. */
static inline int mulib_wl_client_read(mulib_wl_client *c) {
    if (!c || c->failed || c->fd < 0) return -1;
    if (c->eof) return 0;
    if (c->rn >= MULIB_WL_RX_LIMIT) return mulib_wl_client_fail(c);
    unsigned char b[8192];
    size_t room = MULIB_WL_RX_LIMIT - c->rn, nfds = 0;
    if (room > sizeof b) room = sizeof b;
    ssize_t n = mulib_wayland_wire_receive_flags(c->fd, b, room,
        c->fds + c->nfds, MULIB_WL_FD_LIMIT - c->nfds, &nfds, MSG_DONTWAIT);
    if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return 0;
    if (n < 0) return mulib_wl_client_fail(c);
    c->nfds += nfds;
    if (!n) { c->eof = 1; return 1; }
    if (!mulib_wm_grow((void **)&c->rx, &c->rc, c->rn + (size_t)n, 1))
        return mulib_wl_client_fail(c);
    memcpy(c->rx + c->rn, b, (size_t)n); c->rn += (size_t)n;
    return 1;
}
/* The caller keeps sendfd. Native transport duplicates it before queueing and
 * closes the duplicate after the first successful byte (never on EAGAIN). */
static inline int mulib_wl_client_event_fd(mulib_wl_client *c, uint32_t id,
        uint16_t op, const void *p, size_t n, int sendfd) {
    if (!c || c->failed) return 0;
    if (!id || n%4 || n > 65524 || (!p && n) || sendfd < -1 ||
        c->tn > MULIB_WL_RX_LIMIT || n + 8 > MULIB_WL_RX_LIMIT - c->tn)
        return mulib_wl_client_fail(c), 0;
    mulib_wl_tx_entry *q = calloc(1, sizeof *q);
    if (!q) return mulib_wl_client_fail(c), 0;
    if (!mulib_wayland_wire_tx_begin(&q->wire, id, op, p, n, sendfd)) {
        free(q); return mulib_wl_client_fail(c), 0;
    }
    if (c->tail) c->tail->next = q; else c->out = q;
    c->tail = q; c->tn += n + 8;
    return 1;
}
static inline int mulib_wl_client_event(mulib_wl_client *c, uint32_t id,
        uint16_t op, const void *p, size_t n) {
    return mulib_wl_client_event_fd(c, id, op, p, n, -1);
}
/* Register writable readiness only while output is queued. */
static inline int mulib_wl_client_has_output(const mulib_wl_client *c) {
    return c && !c->failed && c->out != NULL;
}
static inline int mulib_wl_client_flush(mulib_wl_client *c) {
    if (!c || c->failed || c->fd < 0) return -1;
    while (c->out) {
        mulib_wl_tx_entry *q = c->out;
        size_t before = q->wire.sent;
        int r = mulib_wayland_wire_tx_step(&q->wire, c->fd);
        c->tn -= q->wire.sent - before;
        if (r < 0) return mulib_wl_client_fail(c);
        if (!r) return 0;
        c->out = q->next;
        mulib_wayland_wire_tx_free(&q->wire); free(q);
    }
    c->tail = NULL; c->tn = 0;
    return 1;
}
#endif
