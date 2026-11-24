/* MuLIB graphical extension: wayland-xdg. C11, header-only. */
#ifndef MULIB_WAYLAND_XDG_H
#define MULIB_WAYLAND_XDG_H
#include "mulib-wayland-wire.h"
static inline int mulib_wayland_xdg_surface(int fd,uint32_t wm,uint32_t xdg,uint32_t surface){if(!xdg||!surface){errno=EINVAL;return 0;}uint32_t a[]={xdg,surface};return mulib_wayland_wire_send(fd,wm,2,a,sizeof a,-1)==0;}
static inline int mulib_wayland_xdg_toplevel(int fd,uint32_t xdg,uint32_t id){if(!id){errno=EINVAL;return 0;}return mulib_wayland_wire_send(fd,xdg,1,&id,4,-1)==0;}
static inline int mulib_wayland_xdg_title(int fd,uint32_t top,const char *title){if(!title){errno=EINVAL;return 0;}mway_bytes b={0};int ok=mway_string(&b,title)&&mulib_wayland_wire_send(fd,top,2,b.v,b.n,-1)==0;mway_bytes_free(&b);return ok;}
static inline int mulib_wayland_xdg_ack(int fd,uint32_t xdg,uint32_t serial){return mulib_wayland_wire_send(fd,xdg,4,&serial,4,-1)==0;}
static inline int mulib_wayland_xdg_pong(int fd,uint32_t wm,uint32_t serial){return mulib_wayland_wire_send(fd,wm,3,&serial,4,-1)==0;}
#endif
