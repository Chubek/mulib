/* MuLIB graphical extension: configured Wayland SHM backend. C11. */
#ifndef MULIB_GLX_BACKEND_WAYLAND_H
#define MULIB_GLX_BACKEND_WAYLAND_H
#include "mulib-wayland-shm.h"
#include "mulib-glx-platform.h"
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
#endif
