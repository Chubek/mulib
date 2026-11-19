/* MuLIB graphical extension: wayland-dmabuf. C11, header-only. */
#ifndef MULIB_WAYLAND_DMABUF_H
#define MULIB_WAYLAND_DMABUF_H
#include "mulib-wayland-wire.h"
static inline int mulib_wayland_dmabuf_params(int fd,uint32_t factory,uint32_t params){if(!params){errno=EINVAL;return 0;}return mulib_wayland_wire_send(fd,factory,1,&params,4,-1)==0;}
static inline int mulib_wayland_dmabuf_plane(int fd,uint32_t params,int dmafd,uint32_t plane,uint32_t offset,uint32_t stride,uint64_t modifier){if(dmafd<0||plane>=4||!stride){errno=EINVAL;return 0;}uint32_t a[]={plane,offset,stride,(uint32_t)(modifier>>32),(uint32_t)modifier};return mulib_wayland_wire_send(fd,params,1,a,sizeof a,dmafd)==0;}
static inline int mulib_wayland_dmabuf_create_immediate(int fd,uint32_t params,uint32_t id,int w,int h,uint32_t format,uint32_t flags){if(!id||w<=0||h<=0||(flags&~7u)){errno=EINVAL;return 0;}uint32_t a[]={id,(uint32_t)w,(uint32_t)h,format,flags};return mulib_wayland_wire_send(fd,params,3,a,sizeof a,-1)==0;}
#endif
