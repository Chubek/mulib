/* MuLIB graphical extension: configured Wayland SHM backend. C11. */
#ifndef MULIB_GLX_BACKEND_WAYLAND_H
#define MULIB_GLX_BACKEND_WAYLAND_H
#include "mulib-wayland-shm.h"
#include "mulib-glx-platform.h"
#include <sys/stat.h>
/* Caller owns connection, configured wl_surface, ARGB8888 wl_buffers and maps.
 * Buffer release is NOT a frame callback or presentation-time notification.
 * This backend does not bootstrap a connection or dispatch protocol events. */
typedef struct {
    int fd;
    uint32_t surface,buffer[2];
    mgx_surface maps[2];
    int busy[2],configured;
    mway_tx tx;
    int pending_slot,failed;
} mulib_glx_backend_wayland_t;
static inline void mulib_glx_backend_wayland_release(mulib_glx_backend_wayland_t *b,uint32_t id) {
    if(!b||!id)return;
    for(int i=0;i<2;i++)if(b->buffer[i]==id) {
        if(!b->tx.bytes.n||b->pending_slot!=i)b->busy[i]=0;
    }
}
static inline int mway_backend_surface_valid(const mgx_surface *s) {
    return s&&s->pixels&&s->w>0&&s->h>0&&s->stride>=(size_t)s->w&&
           s->stride<=SIZE_MAX/sizeof(uint32_t)/(size_t)s->h;
}
/* Ownership/capacity of the borrowed allocation remains the caller's contract. */
static inline int mway_backend_overlap(const mgx_surface *a,const mgx_surface *b) {
    size_t an=a->stride*(size_t)a->h*sizeof(uint32_t);
    size_t bn=b->stride*(size_t)b->h*sizeof(uint32_t);
    uintptr_t ap=(uintptr_t)a->pixels,bp=(uintptr_t)b->pixels;
    if(an>UINTPTR_MAX-ap||bn>UINTPTR_MAX-bp)return 1;
    return ap<bp+bn&&bp<ap+an;
}
static inline mgx_rect mway_backend_clip(mgx_rect r,int w,int h) {
    int64_t x=r.x,y=r.y,right=x+r.w,bottom=y+r.h;
    if(x<0)x=0;
    if(y<0)y=0;
    if(right>w)right=w;
    if(bottom>h)bottom=h;
    if(x>=right||y>=bottom)return (mgx_rect){0,0,0,0};
    return (mgx_rect){(int)x,(int)y,(int)(right-x),(int)(bottom-y)};
}
/* Copies pixels and queues attach/damage/commit without touching the socket.
 * Returns 1 queued, 0 rejected (EAGAIN if all buffers are busy). Pending and
 * committed buffers are immutable until an actual wl_buffer.release event. */
static inline int mulib_glx_backend_wayland_begin(mulib_glx_backend_wayland_t *b,
        const mgx_surface *s,const mgx_rect *damage,size_t n) {
    if(!b||!b->configured||b->failed||b->fd<0||!b->surface||
       !mway_backend_surface_valid(s)||(!damage&&n)||n>4096||
       b->tx.bytes.n||!b->buffer[0]||
       (b->buffer[1]&&b->buffer[0]==b->buffer[1])) { errno=EINVAL; return 0; }
    for(size_t k=0;k<n;k++)if(damage[k].w<0||damage[k].h<0) { errno=EINVAL; return 0; }
    int i=!b->busy[0]?0:b->buffer[1]&&!b->busy[1]?1:-1;
    if(i<0) { errno=EAGAIN; return 0; }
    mgx_surface *map=&b->maps[i];
    if(!mway_backend_surface_valid(map)||map->w!=s->w||map->h!=s->h||
       mway_backend_overlap(s,map)) { errno=EINVAL; return 0; }
    if(b->buffer[1]) {
        const mgx_surface *other=&b->maps[1-i];
        if(!mway_backend_surface_valid(other)||mway_backend_overlap(map,other)) {
            errno=EINVAL; return 0;
        }
    }
    mway_tx tx={0};
    uint32_t attach[]={b->buffer[i],0,0};
    if(!mway_message(&tx.bytes,b->surface,1,attach,sizeof attach))goto fail;
    if(!n) {
        uint32_t full[]={0,0,(uint32_t)s->w,(uint32_t)s->h};
        if(!mway_message(&tx.bytes,b->surface,2,full,sizeof full))goto fail;
    }
    for(size_t k=0;k<n;k++) {
        mgx_rect r=mway_backend_clip(damage[k],s->w,s->h);
        if(!r.w||!r.h)continue;
        uint32_t a[]={(uint32_t)r.x,(uint32_t)r.y,(uint32_t)r.w,(uint32_t)r.h};
        if(!mway_message(&tx.bytes,b->surface,2,a,sizeof a))goto fail;
    }
    if(!mway_message(&tx.bytes,b->surface,6,NULL,0))goto fail;
    /* No mutation or socket traffic occurs before every validation/allocation. */
    for(int y=0;y<s->h;y++)for(int x=0;x<s->w;x++) {
        uint32_t c=s->pixels[(size_t)y*s->stride+(size_t)x];
        unsigned a=c>>24;
        map->pixels[(size_t)y*map->stride+(size_t)x]=(a<<24)|
            ((mgx_ch(c,16)*a+127)/255<<16)|
            ((mgx_ch(c,8)*a+127)/255<<8)|((mgx_ch(c,0)*a+127)/255);
    }
    b->tx=tx; b->pending_slot=i; b->busy[i]=1;
    return 1;
fail:
    mulib_wayland_wire_tx_free(&tx);
    return 0;
}
/* 1 all requests sent (not displayed), 0 EAGAIN, -1 fatal. Fatal errors retain
 * busy ownership conservatively and disable this backend; disconnect first. */
static inline int mulib_glx_backend_wayland_flush(mulib_glx_backend_wayland_t *b) {
    if(!b||b->failed) { errno=EINVAL; return -1; }
    if(!b->tx.bytes.n)return 1;
    int r=mulib_wayland_wire_tx_step(&b->tx,b->fd);
    if(r!=0) {
        int e=errno;
        if(r<0) { b->failed=1; b->configured=0; }
        mulib_wayland_wire_tx_free(&b->tx);
        errno=e;
    }
    return r;
}
/* Cancel/disconnect helper: no maps or socket are freed. Disconnect the borrowed
 * connection before destroying/reusing compositor-owned maps. */
static inline void mulib_glx_backend_wayland_cancel(mulib_glx_backend_wayland_t *b) {
    if(!b)return;
    mulib_wayland_wire_tx_free(&b->tx);
    b->failed=1; b->configured=0;
}
static inline int mulib_glx_backend_wayland_present(void *u,const mgx_surface *s,
        const mgx_rect *damage,size_t n) {
    mulib_glx_backend_wayland_t *b=(mulib_glx_backend_wayland_t *)u;
    if(!mulib_glx_backend_wayland_begin(b,s,damage,n))return 0;
    int r;
    while((r=mulib_glx_backend_wayland_flush(b))==0) {
        if(mway_wait(b->fd,POLLOUT)<0) {
            mulib_glx_backend_wayland_cancel(b); return 0;
        }
    }
    return r==1;
}

/* Optional, bounded single-toplevel client. The configured backend above remains
 * independently usable. This client owns its socket and transport queues, NOT
 * the supplied SHM maps. No seat/output/data/DMABUF globals are bound.
 *
 * Zero initialize; never copy, reenter or share across threads. All events are
 * polled state, not callbacks. Configure dimensions are logical pixels at scale
 * one. Frame done is pacing, NOT presentation feedback or buffer release. */
typedef struct {
    mulib_glx_backend_wayland_t backend;
    mway_stream input;
    mway_tx control;
    uint32_t globals[3], next_id, retired[64];
    size_t retired_n, global_events, max_pool_bytes;
    uint32_t frame_id, frame_time;
    uint64_t frame_token;
    int active, state, failed, close_requested, argb;
    int width, height, proposed_width, proposed_height, have_toplevel;
    int resize_pending, frame_ready;
    char title[256];
} mulib_glx_backend_wayland_client_t;

enum {
    MWAY_CLIENT_REGISTRY = 1,
    MWAY_CLIENT_CONFIGURE = 2,
    MWAY_CLIENT_READY = 3
};

static inline void mulib_glx_backend_wayland_client_free(
    mulib_glx_backend_wayland_client_t *c)
{
    if (!c)
        return;
    /* Disconnect before making borrowed compositor-read storage reusable. */
    if (c->active && c->backend.fd >= 0)
        close(c->backend.fd);
    mulib_glx_backend_wayland_cancel(&c->backend);
    mulib_wayland_wire_tx_free(&c->control);
    mulib_wayland_wire_stream_free(&c->input);
    memset(c, 0, sizeof *c);
    c->backend.fd = -1;
}

static inline int mway_client_fail(mulib_glx_backend_wayland_client_t *c,
                                   int error)
{
    if (c->active && c->backend.fd >= 0)
        close(c->backend.fd);
    c->backend.fd = -1;
    c->failed = 1;
    mulib_glx_backend_wayland_cancel(&c->backend);
    mulib_wayland_wire_tx_free(&c->control);
    mulib_wayland_wire_stream_free(&c->input);
    errno = error;
    return -1;
}

/* Success transfers fd ownership. Failure leaves fd caller-owned. No I/O is
 * performed. Limits cover pool bytes, 256 registry announcements, 64 pending
 * delete_id acknowledgements, 64 KiB control TX and 256 KiB RX. */
static inline int mulib_glx_backend_wayland_client_init(
    mulib_glx_backend_wayland_client_t *c, int fd, const char *title,
    int width, int height, size_t max_pool_bytes)
{
    if (!c || c->active || fd < 0 || !title || strlen(title) > 255 ||
        width <= 0 || height <= 0 || width > 8192 || height > 8192 ||
        !max_pool_bytes || max_pool_bytes > INT32_MAX) {
        errno = EINVAL;
        return 0;
    }
    mway_tx tx = {0};
    uint32_t registry = 2, sync = 3;
    if (!mway_message(&tx.bytes, 1, 1, &registry, 4) ||
        !mway_message(&tx.bytes, 1, 0, &sync, 4)) {
        mulib_wayland_wire_tx_free(&tx);
        return 0;
    }
    if (mway_cloexec(fd) < 0) {
        mulib_wayland_wire_tx_free(&tx);
        return 0;
    }
    *c = (mulib_glx_backend_wayland_client_t){
        .backend = {.fd = fd, .surface = 7},
        .control = tx,
        .next_id = 9,
        .max_pool_bytes = max_pool_bytes,
        .active = 1,
        .state = MWAY_CLIENT_REGISTRY,
        .width = width,
        .height = height};
    memcpy(c->title, title, strlen(title) + 1);
    return 1;
}

/* Explicit trusted Unix socket path only; no discovery, autostart, or access to
 * the active desktop unless the host explicitly supplies its socket. connect()
 * itself is synchronous; all subsequent I/O uses the nonblocking step APIs. */
static inline int mulib_glx_backend_wayland_client_connect(
    mulib_glx_backend_wayland_client_t *c, const char *path, const char *title,
    int width, int height, size_t max_pool_bytes)
{
    if (!c || c->active) {
        errno = EINVAL;
        return 0;
    }
    int fd = mulib_wayland_wire_connect(path);
    if (fd < 0)
        return 0;
    if (mulib_glx_backend_wayland_client_init(
            c, fd, title, width, height, max_pool_bytes))
        return 1;
    int error = errno;
    close(fd);
    errno = error;
    return 0;
}

static inline int mway_client_retire(mulib_glx_backend_wayland_client_t *c,
                                     uint32_t id)
{
    if (!id || c->retired_n == 64)
        return 0;
    c->retired[c->retired_n++] = id;
    return 1;
}

static inline int mway_client_control(mulib_glx_backend_wayland_client_t *c,
                                      uint32_t object, uint16_t opcode,
                                      const void *args, size_t n)
{
    if (n > 65524 || c->control.bytes.n > 65536 - n - 8)
        return mway_client_fail(c, ENOBUFS);
    if (!mway_message(&c->control.bytes, object, opcode, args, n))
        return mway_client_fail(c, ENOMEM);
    return 1;
}

static inline int mway_client_bind(mway_bytes *bytes, uint32_t global,
                                   const char *name, uint32_t id)
{
    mway_bytes args = {0};
    int ok = mway_u32(&args, global) && mway_string(&args, name) &&
             mway_u32(&args, 1) && mway_u32(&args, id) &&
             mway_message(bytes, 2, 0, args.v, args.n);
    mway_bytes_free(&args);
    return ok;
}

static inline int mway_client_bootstrap(mulib_glx_backend_wayland_client_t *c)
{
    if (!c->globals[0] || !c->globals[1] || !c->globals[2])
        return mway_client_fail(c, ENOTSUP);
    mway_bytes bytes = {0}, title = {0};
    uint32_t surface = 7, xdg[] = {8, 7}, top = 9;
    int ok = mway_client_bind(&bytes, c->globals[0], "wl_compositor", 4) &&
             mway_client_bind(&bytes, c->globals[1], "wl_shm", 5) &&
             mway_client_bind(&bytes, c->globals[2], "xdg_wm_base", 6) &&
             mway_message(&bytes, 4, 0, &surface, 4) &&
             mway_message(&bytes, 6, 2, xdg, sizeof xdg) &&
             mway_message(&bytes, 8, 1, &top, 4) &&
             mway_string(&title, c->title) &&
             mway_message(&bytes, 9, 2, title.v, title.n) &&
             mway_message(&bytes, 7, 6, NULL, 0);
    mway_bytes_free(&title);
    if (!ok || c->control.has_fd ||
        bytes.n > 65536 - c->control.bytes.n ||
        !mway_append(&c->control.bytes, bytes.v, bytes.n)) {
        mway_bytes_free(&bytes);
        return mway_client_fail(c, ENOMEM);
    }
    mway_bytes_free(&bytes);
    c->state = MWAY_CLIENT_CONFIGURE;
    return 1;
}

static inline uint32_t mway_client_u32(const unsigned char *p)
{
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

/* All events on the negotiated v1 interfaces are FD-free. Unknown objects,
 * unexpected events, malformed arities and duplicate releases are fatal. */
static inline int mway_client_event(mulib_glx_backend_wayland_client_t *c,
                                    const mway_frame *f)
{
    if (f->object == 1) {
        if (f->opcode == 0)
            return mway_client_fail(c, EPROTO); /* wl_display.error */
        if (f->opcode != 1 || f->n != 4)
            return mway_client_fail(c, EPROTO);
        uint32_t id = mway_client_u32(f->args);
        for (size_t i = 0; i < c->retired_n; i++)
            if (c->retired[i] == id) {
                c->retired[i] = c->retired[--c->retired_n];
                return 1;
            }
        return mway_client_fail(c, EPROTO);
    }
    if (f->object == 2) {
        if (f->opcode == 1 && f->n == 4) {
            uint32_t name = mway_client_u32(f->args);
            for (size_t i = 0; i < 3; i++)
                if (c->globals[i] == name)
                    return mway_client_fail(c, ENODEV);
            return 1;
        }
        if (f->opcode || ++c->global_events > 256 || f->n < 16)
            return mway_client_fail(c, EPROTO);
        uint32_t name = mway_client_u32(f->args);
        uint32_t length = mway_client_u32(f->args + 4);
        if (!length || length > f->n - 12)
            return mway_client_fail(c, EPROTO);
        size_t padded = ((size_t)length + 3) & ~(size_t)3;
        if (padded != f->n - 12 || f->args[8 + length - 1] ||
            memchr(f->args + 8, 0, length - 1))
            return mway_client_fail(c, EPROTO);
        uint32_t version = mway_client_u32(f->args + 8 + padded);
        if (!name || !version)
            return mway_client_fail(c, EPROTO);
        const char *names[] = {"wl_compositor", "wl_shm", "xdg_wm_base"};
        for (size_t i = 0; i < 3; i++)
            if (!c->globals[i] && !strcmp((const char *)f->args + 8, names[i]))
                c->globals[i] = name;
        return 1;
    }
    if (f->object == 3 && c->state == MWAY_CLIENT_REGISTRY &&
        !f->opcode && f->n == 4) {
        if (!mway_client_retire(c, 3))
            return mway_client_fail(c, ENOBUFS);
        return mway_client_bootstrap(c);
    }
    if (c->state == MWAY_CLIENT_REGISTRY)
        return mway_client_fail(c, EPROTO);
    if (f->object == 5 && !f->opcode && f->n == 4) {
        if (!mway_client_u32(f->args))
            c->argb = 1;
        return 1;
    }
    if (f->object == 6 && !f->opcode && f->n == 4)
        return mway_client_control(c, 6, 3, f->args, 4);
    if (f->object == 9 && !f->opcode) {
        if (f->n < 12 || mway_client_u32(f->args + 8) != f->n - 12 ||
            mway_client_u32(f->args + 8) % 4 ||
            mway_client_u32(f->args + 8) > 1024)
            return mway_client_fail(c, EPROTO);
        int32_t w, h;
        memcpy(&w, f->args, 4);
        memcpy(&h, f->args + 4, 4);
        if (w < 0 || h < 0 || w > 8192 || h > 8192)
            return mway_client_fail(c, EPROTO);
        c->proposed_width = w ? w : c->width;
        c->proposed_height = h ? h : c->height;
        c->have_toplevel = 1;
        return 1;
    }
    if (f->object == 9 && f->opcode == 1 && !f->n) {
        c->close_requested = 1;
        c->backend.configured = 0;
        return 1;
    }
    if (f->object == 8 && !f->opcode && f->n == 4) {
        if (mway_client_control(c, 8, 4, f->args, 4) < 0)
            return -1;
        if (c->have_toplevel) {
            c->width = c->proposed_width;
            c->height = c->proposed_height;
        }
        c->have_toplevel = 0;
        c->resize_pending = !c->backend.buffer[0] ||
                            c->backend.maps[0].w != c->width ||
                            c->backend.maps[0].h != c->height;
        c->backend.configured = !c->resize_pending && !c->close_requested;
        c->state = MWAY_CLIENT_READY;
        return 1;
    }
    for (size_t i = 0; i < 2; i++)
        if (f->object == c->backend.buffer[i] && f->object) {
            if (f->opcode || f->n || !c->backend.busy[i] ||
                (c->backend.tx.bytes.n && c->backend.pending_slot == (int)i))
                return mway_client_fail(c, EPROTO);
            mulib_glx_backend_wayland_release(&c->backend, f->object);
            return 1;
        }
    if (f->object == c->frame_id && c->frame_id) {
        if (f->opcode || f->n != 4 || c->backend.tx.bytes.n ||
            !mway_client_retire(c, c->frame_id))
            return mway_client_fail(c, EPROTO);
        c->frame_id = 0;
        c->frame_time = mway_client_u32(f->args);
        c->frame_ready = 1;
        return 1;
    }
    return mway_client_fail(c, EPROTO);
}

/* Receive at most 4096 bytes: 1 progress/EOF queued, 0 EAGAIN, -1 fatal.
 * Dispatch buffered events even after EOF; dispatch then reports disconnect.
 * No other reader or libwayland dispatcher may use this connection. */
static inline int mulib_glx_backend_wayland_client_receive(
    mulib_glx_backend_wayland_client_t *c)
{
    if (!c || !c->active || c->failed) {
        errno = EINVAL;
        return -1;
    }
    ssize_t n = mulib_wayland_wire_stream_receive(&c->input, c->backend.fd);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return mway_client_fail(c, errno);
    }
    if (c->input.fd_n)
        return mway_client_fail(c, EPROTO);
    return 1;
}

/* Returns number dispatched, or -1 fatal. Call again when budget is exhausted,
 * even without new socket readiness: already-buffered frames remain runnable. */
static inline int mulib_glx_backend_wayland_client_dispatch(
    mulib_glx_backend_wayland_client_t *c, unsigned budget)
{
    if (!c || !c->active || c->failed || !budget || budget > 1024) {
        errno = EINVAL;
        return -1;
    }
    unsigned count = 0;
    while (count < budget) {
        mway_frame f;
        int r = mulib_wayland_wire_stream_peek(&c->input, &f);
        if (r < 0)
            return mway_client_fail(c, errno);
        if (!r) {
            if (c->input.eof && !count)
                return mway_client_fail(c, ECONNRESET);
            break;
        }
        /* No callbacks; event reads its borrowed payload before consume. */
        if (mway_client_event(c, &f) < 0)
            return -1;
        if (mulib_wayland_wire_stream_consume(&c->input, 0, NULL) != 1)
            return mway_client_fail(c, EPROTO);
        count++;
    }
    return (int)count;
}

/* Flush requests without interleaving partial control and presentation batches.
 * 1 drained, 0 EAGAIN, -1 fatal (socket closed). Subscribe to writable readiness
 * only while either queue has bytes. Transport completion is not frame done. */
static inline int mulib_glx_backend_wayland_client_flush(
    mulib_glx_backend_wayland_client_t *c)
{
    if (!c || !c->active || c->failed) {
        errno = EINVAL;
        return -1;
    }
    /* begin() requires an empty control queue. Therefore any control now queued
     * alongside a frame was generated later: finish that frame before newer
     * configure acknowledgements, even if the frame has not sent its first byte. */
    if (c->backend.tx.bytes.n) {
        int r = mulib_glx_backend_wayland_flush(&c->backend);
        if (r < 0)
            return mway_client_fail(c, errno);
        if (!r)
            return 0;
    }
    if (c->control.bytes.n) {
        int r = mulib_wayland_wire_tx_step(&c->control, c->backend.fd);
        if (r < 0)
            return mway_client_fail(c, errno);
        if (!r)
            return 0;
        mulib_wayland_wire_tx_free(&c->control);
    }
    return 1;
}

/* Host supplies two disjoint writable mappings of this FD at byte offsets,
 * and their actual capacities in words. They must remain mapped and immutable
 * while busy, and outlive the client or a successful replacement. Mapping/FD
 * identity cannot be inferred from pointers: the host guarantees that relation.
 * The FD is duplicated for TX; source FD ownership never transfers.
 *
 * Retry EBUSY after frame/release and pending TX. No socket I/O here. Replacing
 * busy buffers is rejected; old buffer destroy precedes new pool creation.
 * New IDs are never reused, and deletion bookkeeping is independently bounded. */
static inline int mulib_glx_backend_wayland_client_buffers(
    mulib_glx_backend_wayland_client_t *c, int memfd, size_t pool_bytes,
    const int offsets[2], const mgx_surface maps[2], const size_t words[2])
{
    if (!c || !c->active || c->failed || c->close_requested ||
        c->state != MWAY_CLIENT_READY || !c->argb || memfd < 0 ||
        !offsets || !maps || !words || !pool_bytes ||
        pool_bytes > c->max_pool_bytes) {
        errno = EINVAL;
        return 0;
    }
    if (c->backend.busy[0] || c->backend.busy[1] || c->frame_id ||
        c->frame_ready || c->backend.tx.bytes.n || c->control.bytes.n ||
        c->retired_n > 61) {
        errno = EBUSY;
        return 0;
    }
    if (c->next_id > 0xfefffffb) {
        errno = ENOSPC;
        return 0;
    }
    struct stat st;
    if (fstat(memfd, &st) < 0)
        return 0;
    if (st.st_size < 0 || (uint64_t)st.st_size < pool_bytes) {
        errno = EINVAL;
        return 0;
    }
    size_t extents[2];
    for (size_t i = 0; i < 2; i++) {
        if (!mgx_surface_valid(&maps[i]) || offsets[i] % 4 ||
            maps[i].w != c->width ||
            maps[i].h != c->height || maps[i].stride > INT32_MAX / 4 ||
            maps[i].stride > words[i] / (size_t)maps[i].h ||
            maps[i].stride * (size_t)maps[i].h >
                (UINTPTR_MAX - (uintptr_t)maps[i].pixels) / sizeof(uint32_t) ||
            !mulib_wayland_shm_buffer_extent(
                offsets[i], maps[i].w, maps[i].h, (int)maps[i].stride * 4,
                0, &extents[i]) || extents[i] > pool_bytes) {
            errno = EINVAL;
            return 0;
        }
    }
    if (offsets[0] < (int64_t)extents[1] &&
        offsets[1] < (int64_t)extents[0]) {
        errno = EINVAL;
        return 0;
    }
    if (mway_backend_overlap(&maps[0], &maps[1])) {
        errno = EINVAL;
        return 0;
    }
    mway_tx tx = {0};
    uint32_t pool = c->next_id + 1, ids[] = {pool + 1, pool + 2};
    for (size_t i = 0; i < 2; i++)
        if (c->backend.buffer[i] &&
            !mway_message(&tx.bytes, c->backend.buffer[i], 0, NULL, 0))
            goto fail;
    uint32_t args[] = {pool, (uint32_t)pool_bytes};
    if (!mway_message(&tx.bytes, 5, 0, args, sizeof args))
        goto fail;
    for (size_t i = 0; i < 2; i++) {
        uint32_t a[] = {ids[i], (uint32_t)offsets[i], (uint32_t)c->width,
                        (uint32_t)c->height, (uint32_t)maps[i].stride * 4, 0};
        if (!mway_message(&tx.bytes, pool, 0, a, sizeof a))
            goto fail;
    }
    if (!mway_message(&tx.bytes, pool, 1, NULL, 0))
        goto fail;
    tx.sendfd = mway_dup_cloexec(memfd);
    if (tx.sendfd < 0)
        goto fail;
    tx.has_fd = 1;
    for (size_t i = 0; i < 2; i++) {
        if (c->backend.buffer[i])
            (void)mway_client_retire(c, c->backend.buffer[i]);
        c->backend.buffer[i] = ids[i];
        c->backend.maps[i] = maps[i];
    }
    (void)mway_client_retire(c, pool);
    c->next_id = ids[1];
    c->control = tx;
    c->backend.configured = 1;
    c->resize_pending = 0;
    return 1;
fail:
    mulib_wayland_wire_tx_free(&tx);
    return 0;
}

/* Queues a copied frame and one wl_surface.frame callback. token is caller
 * identity (e.g. the next mulib_glx_present serial), never a wire object ID.
 * At most one frame callback is outstanding; releases remain independent. */
static inline int mulib_glx_backend_wayland_client_begin(
    mulib_glx_backend_wayland_client_t *c, const mgx_surface *source,
    const mgx_rect *damage, size_t n, uint64_t token)
{
    if (!c || !c->active || c->failed || c->close_requested || !token ||
        !source || source->w != c->width || source->h != c->height) {
        errno = EINVAL;
        return 0;
    }
    if (c->control.bytes.n || c->frame_id || c->frame_ready ||
        c->retired_n == 64) {
        errno = EAGAIN;
        return 0;
    }
    if (c->next_id == 0xfeffffff) {
        errno = ENOSPC;
        return 0;
    }
    /* Reserve the frame request before begin copies pixels into a free map.
     * The backend allocates its own complete attach/damage/commit batch. */
    mway_bytes frame = {0};
    uint32_t id = c->next_id + 1;
    if (!mway_message(&frame, 7, 3, &id, 4)) {
        mway_bytes_free(&frame);
        return 0;
    }
    if (!mulib_glx_backend_wayland_begin(&c->backend, source, damage, n)) {
        mway_bytes_free(&frame);
        return 0;
    }
    mway_bytes *bytes = &c->backend.tx.bytes;
    /* Append may allocate. On failure no bytes have been sent: roll back slot
     * ownership, preserving logical submission/token state for an exact retry. */
    size_t old = bytes->n;
    bytes->n -= 8;
    if (!mway_append(bytes, frame.v, frame.n) ||
        !mway_message(bytes, 7, 6, NULL, 0)) {
        bytes->n = old;
        c->backend.busy[c->backend.pending_slot] = 0;
        mulib_wayland_wire_tx_free(&c->backend.tx);
        mway_bytes_free(&frame);
        return 0;
    }
    mway_bytes_free(&frame);
    c->next_id = id;
    c->frame_id = id;
    c->frame_token = token;
    return 1;
}

static inline int mulib_glx_backend_wayland_client_take_frame(
    mulib_glx_backend_wayland_client_t *c, uint64_t *token, uint32_t *milliseconds)
{
    if (!c || !token || !milliseconds || !c->frame_ready || c->failed)
        return 0;
    *token = c->frame_token;
    *milliseconds = c->frame_time;
    c->frame_ready = 0;
    c->frame_token = 0;
    return 1;
}
#endif
