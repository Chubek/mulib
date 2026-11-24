/* MuLIB graphical extension: x11-selection. C11, header-only. */
#ifndef MULIB_X11_SELECTION_H
#define MULIB_X11_SELECTION_H
#include "mulib-x11-wire.h"
static inline int mulib_x11_selection_owner(mx11_connection *c,uint32_t window,uint32_t atom,uint32_t time){if(!atom){errno=EINVAL;return 0;}unsigned char p[12];mx11_put32(p,window);mx11_put32(p+4,atom);mx11_put32(p+8,time);return mulib_x11_wire_request(c,22,0,p,12);}
static inline int mulib_x11_selection_convert(mx11_connection *c,uint32_t requestor,uint32_t selection,uint32_t target,uint32_t property,uint32_t time){if(!requestor||!selection||!target){errno=EINVAL;return 0;}unsigned char p[20];mx11_put32(p,requestor);mx11_put32(p+4,selection);mx11_put32(p+8,target);mx11_put32(p+12,property);mx11_put32(p+16,time);return mulib_x11_wire_request(c,24,0,p,20);}
#endif
