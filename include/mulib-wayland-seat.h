/* MuLIB graphical extension: wayland-seat. C11, header-only. */
#ifndef MULIB_WAYLAND_SEAT_H
#define MULIB_WAYLAND_SEAT_H
#include "mulib-wayland-wire.h"
#include "mulib-glx-event.h"
static inline int mulib_wayland_seat_pointer(int fd,uint32_t seat,uint32_t id){if(!id){errno=EINVAL;return 0;}return mulib_wayland_wire_send(fd,seat,0,&id,4,-1)==0;}
static inline int mulib_wayland_seat_keyboard(int fd,uint32_t seat,uint32_t id){if(!id){errno=EINVAL;return 0;}return mulib_wayland_wire_send(fd,seat,1,&id,4,-1)==0;}
static inline int mulib_wayland_seat_touch(int fd,uint32_t seat,uint32_t id){if(!id){errno=EINVAL;return 0;}return mulib_wayland_wire_send(fd,seat,2,&id,4,-1)==0;}
static inline double mulib_wayland_seat_fixed(uint32_t v){return (int32_t)v/256.;}
static inline int mulib_wayland_seat_motion(const mway_frame *f,mgx_event *e){if(!f||!e||!f->args||f->opcode!=2||f->n!=12)return 0;uint32_t a[3];memcpy(a,f->args,12);memset(e,0,sizeof *e);e->type=MGX_EVENT_MOTION;e->time_us=(uint64_t)a[0]*1000;e->x=mulib_wayland_seat_fixed(a[1]);e->y=mulib_wayland_seat_fixed(a[2]);return 1;}
#endif
