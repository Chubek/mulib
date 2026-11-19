/* MuLIB graphical extension: single-thread-owned, mutation-safe poll step. */
#ifndef MULIB_GLX_EVENTLOOP_H
#define MULIB_GLX_EVENTLOOP_H
#include "mulib-glx-event.h"
#include <poll.h>
typedef void (*mgx_io_fn)(void *,short);
typedef struct{struct pollfd p;mgx_io_fn fn;void *user;uint64_t id;}mgx_watch;
typedef struct {
    mgx_watch *v;
    size_t n, cap;
    int running;
    size_t limit;
    uint64_t next_id;
} mulib_glx_eventloop_t;

#ifndef MULIB_GLX_EVENTLOOP_LIMIT
#define MULIB_GLX_EVENTLOOP_LIMIT 1024u
#endif
#ifndef MULIB_GLX_EVENTLOOP_MALLOC
#define MULIB_GLX_EVENTLOOP_MALLOC malloc
#endif
#ifndef MULIB_GLX_EVENTLOOP_REALLOC
#define MULIB_GLX_EVENTLOOP_REALLOC realloc
#endif
#ifndef MULIB_GLX_EVENTLOOP_FREE
#define MULIB_GLX_EVENTLOOP_FREE free
#endif

static inline int mulib_glx_eventloop_set_limit(mulib_glx_eventloop_t *l, size_t limit)
{
    if (!l || !limit || limit > INT_MAX || limit < l->n
        || limit > SIZE_MAX / sizeof(mgx_watch)) {
        errno = EINVAL;
        return 0;
    }
    l->limit = limit;
    return 1;
}

/* Borrow fd and user; callbacks may add/remove watches. Duplicate fds fail. */
static inline int mulib_glx_eventloop_add(mulib_glx_eventloop_t *l, int fd,
                                         short events, mgx_io_fn fn, void *u)
{
    size_t limit;
    if (!l || fd < 0 || !fn || !events
        || (events & ~(POLLIN | POLLOUT | POLLPRI))
        || l->n > l->cap || (l->cap && !l->v)) {
        errno = EINVAL;
        return 0;
    }
    for (size_t i = 0; i < l->n; ++i)
        if (l->v[i].p.fd == fd) { errno = EEXIST; return 0; }
    limit = l->limit ? l->limit : MULIB_GLX_EVENTLOOP_LIMIT;
    if (l->n >= limit || l->n >= INT_MAX
        || l->n >= SIZE_MAX / sizeof *l->v) {
        errno = ENOSPC;
        return 0;
    }
    if (l->next_id == UINT64_MAX) { errno = EOVERFLOW; return 0; }
    if (l->n == l->cap) {
        size_t cap = l->cap ? l->cap : 8;
        mgx_watch *v;
        if (cap <= l->n) cap = l->n > SIZE_MAX / 2 ? limit : l->n * 2;
        if (cap > limit) cap = limit;
        if (cap > SIZE_MAX / sizeof *v) cap = SIZE_MAX / sizeof *v;
        v = (mgx_watch *)MULIB_GLX_EVENTLOOP_REALLOC(l->v, cap * sizeof *v);
        if (!v) { errno = ENOMEM; return 0; }
        l->v = v;
        l->cap = cap;
    }
    l->v[l->n++] = (mgx_watch){{fd, events, 0}, fn, u, ++l->next_id};
    return 1;
}

static inline int mulib_glx_eventloop_remove(mulib_glx_eventloop_t *l, int fd)
{
    if (!l || fd < 0 || l->n > l->cap || (l->n && !l->v)) {
        errno = EINVAL;
        return 0;
    }
    for (size_t i = 0; i < l->n; ++i) {
        if (l->v[i].p.fd != fd) continue;
        memmove(l->v + i, l->v + i + 1, (l->n - i - 1) * sizeof *l->v);
        --l->n;
        return 1;
    }
    errno = ENOENT;
    return 0;
}

/* A removed/re-added fd has a new identity and cannot receive old readiness.
 * New watches start on the next poll. Recursive poll/free fail with EBUSY.
 * HUP/ERR/NVAL watches detach before their final callback to prevent EOF spins.
 * Returns poll's readiness count (including removed watches), not call count.
 * EINTR is returned, never retried internally or converted to a zero-time spin. */
static inline int mulib_glx_eventloop_poll(mulib_glx_eventloop_t *l, int timeout)
{
    struct pollfd *p = NULL;
    mgx_watch *snapshot = NULL;
    size_t n;
    int rc, saved;
    if (!l || timeout < -1 || l->n > l->cap || (l->n && !l->v)
        || l->n > INT_MAX || l->n > SIZE_MAX / sizeof *snapshot
        || l->n > SIZE_MAX / sizeof *p) {
        errno = EINVAL;
        return -1;
    }
    if (l->running) { errno = EBUSY; return -1; }
    n = l->n;
    if (n) {
        p = (struct pollfd *)MULIB_GLX_EVENTLOOP_MALLOC(n * sizeof *p);
        if (!p) { errno = ENOMEM; return -1; }
        snapshot = (mgx_watch *)MULIB_GLX_EVENTLOOP_MALLOC(n * sizeof *snapshot);
        if (!snapshot) {
            MULIB_GLX_EVENTLOOP_FREE(p);
            errno = ENOMEM;
            return -1;
        }
        memcpy(snapshot, l->v, n * sizeof *snapshot);
        for (size_t i = 0; i < n; ++i) p[i] = snapshot[i].p;
    }
    l->running = 1;
    rc = poll(p, (nfds_t)n, timeout);
    saved = errno;
    if (rc > 0) {
        for (size_t i = 0; i < n; ++i) {
            if (!p[i].revents) continue;
            for (size_t j = 0; j < l->n; ++j) {
                if (l->v[j].id != snapshot[i].id) continue;
                if (p[i].revents & (POLLHUP | POLLERR | POLLNVAL))
                    (void)mulib_glx_eventloop_remove(l, snapshot[i].p.fd);
                snapshot[i].fn(snapshot[i].user, p[i].revents);
                break;
            }
        }
    }
    l->running = 0;
    MULIB_GLX_EVENTLOOP_FREE(snapshot);
    MULIB_GLX_EVENTLOOP_FREE(p);
    if (rc < 0) errno = saved;
    return rc;
}

static inline void mulib_glx_eventloop_free(mulib_glx_eventloop_t *l)
{
    if (!l) return;
    if (l->running) { errno = EBUSY; return; }
    MULIB_GLX_EVENTLOOP_FREE(l->v);
    memset(l, 0, sizeof *l);
}
#endif
