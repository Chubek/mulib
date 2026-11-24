/* MuLIB graphical extension: configured Wayland SHM requests. C11. */
#ifndef MULIB_WAYLAND_SHM_H
#define MULIB_WAYLAND_SHM_H
#include "mulib-wayland-wire.h"
#include <sys/mman.h>
static inline int mulib_wayland_shm_pool(int fd,uint32_t shm,uint32_t pool,int memfd,int32_t size) {
    if(!pool||memfd<0||size<=0) { errno=EINVAL; return 0; }
    uint32_t a[]={pool,(uint32_t)size};
    return mulib_wayland_wire_send(fd,shm,0,a,sizeof a,memfd)==0;
}
/* This four-byte-pixel helper supports wl_shm ARGB8888(0)/XRGB8888(1) only.
 * The declared pool must cover offset + height * stride, including padding. */
static inline int mulib_wayland_shm_buffer_extent(int offset,int w,int h,int stride,
        uint32_t format,size_t *required) {
    if(!required||offset<0||w<=0||h<=0||stride<=0||stride%4||
       (int64_t)stride<(int64_t)w*4||format>1) { errno=EINVAL; return 0; }
    uint64_t end=(uint64_t)(unsigned)offset+
        (uint64_t)(unsigned)stride*(unsigned)h;
    if(end>INT32_MAX||end>SIZE_MAX) { errno=EOVERFLOW; return 0; }
    *required=(size_t)end;
    return 1;
}
static inline int mulib_wayland_shm_buffer(int fd,uint32_t pool,uint32_t id,
        int offset,int w,int h,int stride,uint32_t format) {
    size_t required;
    if(!id||!mulib_wayland_shm_buffer_extent(offset,w,h,stride,format,&required))return 0;
    uint32_t a[]={id,(uint32_t)offset,(uint32_t)w,(uint32_t)h,(uint32_t)stride,format};
    return mulib_wayland_wire_send(fd,pool,0,a,sizeof a,-1)==0;
}
/* Use this variant when pool capacity is known; old API cannot verify it. */
static inline int mulib_wayland_shm_buffer_bounded(int fd,uint32_t pool,uint32_t id,
        int offset,int w,int h,int stride,uint32_t format,size_t pool_bytes) {
    size_t required;
    if(!mulib_wayland_shm_buffer_extent(offset,w,h,stride,format,&required))return 0;
    if(required>pool_bytes||pool_bytes>INT32_MAX) { errno=EINVAL; return 0; }
    return mulib_wayland_shm_buffer(fd,pool,id,offset,w,h,stride,format);
}
static inline int mulib_wayland_shm_attach(int fd,uint32_t surface,uint32_t buffer,int x,int y) {
    uint32_t a[]={buffer,(uint32_t)x,(uint32_t)y};
    return mulib_wayland_wire_send(fd,surface,1,a,sizeof a,-1)==0;
}
static inline int mulib_wayland_shm_commit(int fd,uint32_t surface) {
    return mulib_wayland_wire_send(fd,surface,6,NULL,0,-1)==0;
}
static inline int mulib_wayland_shm_damage(int fd,uint32_t surface,mgx_rect r) {
    if(r.w<0||r.h<0) { errno=EINVAL; return 0; }
    uint32_t a[]={(uint32_t)r.x,(uint32_t)r.y,(uint32_t)r.w,(uint32_t)r.h};
    return mulib_wayland_wire_send(fd,surface,2,a,sizeof a,-1)==0;
}
#endif
