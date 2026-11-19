/* MuLIB graphical extension: wayland-data. C11, header-only. */
#ifndef MULIB_WAYLAND_DATA_H
#define MULIB_WAYLAND_DATA_H
#include "mulib-wayland-wire.h"
static inline int mulib_wayland_data_offer(int fd,uint32_t source,const char *mime){if(!mime||!*mime){errno=EINVAL;return 0;}mway_bytes b={0};int ok=mway_string(&b,mime)&&mulib_wayland_wire_send(fd,source,0,b.v,b.n,-1)==0;mway_bytes_free(&b);return ok;}
static inline int mulib_wayland_data_selection(int fd,uint32_t device,uint32_t source,uint32_t serial){uint32_t a[]={source,serial};return mulib_wayland_wire_send(fd,device,1,a,sizeof a,-1)==0;}
static inline int mulib_wayland_data_receive(int fd,uint32_t offer,const char *mime,int write_fd){if(!mime||!*mime||write_fd<0){errno=EINVAL;return 0;}mway_bytes b={0};int ok=mway_string(&b,mime)&&mulib_wayland_wire_send(fd,offer,1,b.v,b.n,write_fd)==0;mway_bytes_free(&b);return ok;}
#endif
