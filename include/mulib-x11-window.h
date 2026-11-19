/* MuLIB graphical extension: x11-window. C11, header-only. */
#ifndef MULIB_X11_WINDOW_H
#define MULIB_X11_WINDOW_H
#include "mulib-x11-wire.h"
static inline int mulib_x11_window_create(mx11_connection *c,uint32_t id,uint32_t parent,int16_t x,int16_t y,uint16_t w,uint16_t h,uint32_t eventmask){if(!id||!parent||!w||!h)return 0;unsigned char p[32]={0};mx11_put32(p,id);mx11_put32(p+4,parent);mx11_put16(p+8,(uint16_t)x);mx11_put16(p+10,(uint16_t)y);mx11_put16(p+12,w);mx11_put16(p+14,h);mx11_put16(p+18,1);mx11_put32(p+24,1u<<11);mx11_put32(p+28,eventmask);return mulib_x11_wire_request(c,1,0,p,sizeof p);}
static inline int mulib_x11_window_map(mx11_connection *c,uint32_t w){if(!w){errno=EINVAL;return 0;}unsigned char p[4];mx11_put32(p,w);return mulib_x11_wire_request(c,8,0,p,4);}
static inline int mulib_x11_window_destroy(mx11_connection *c,uint32_t w){if(!w){errno=EINVAL;return 0;}unsigned char p[4];mx11_put32(p,w);return mulib_x11_wire_request(c,4,0,p,4);}
static inline int mulib_x11_window_gc(mx11_connection *c,uint32_t id,uint32_t drawable){if(!id||!drawable){errno=EINVAL;return 0;}unsigned char p[12]={0};mx11_put32(p,id);mx11_put32(p+4,drawable);return mulib_x11_wire_request(c,55,0,p,12);}
#endif
