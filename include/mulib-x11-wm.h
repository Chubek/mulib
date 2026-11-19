/* MuLIB graphical extension: bounded X11 property requests. C11. */
#ifndef MULIB_X11_WM_H
#define MULIB_X11_WM_H
#include "mulib-x11-wire.h"
/* data contains wire-order elements, not endian-converted host integers. */
static inline int mulib_x11_wm_property(mx11_connection *c,uint32_t w,uint32_t property,
        uint32_t type,uint8_t format,const void *data,size_t n) {
    size_t unit=format==8?1:format==16?2:format==32?4:0;
    if(!c||c->failed||!w||!property||!type||!unit||n%unit||
       n>262000||(!data&&n)) { errno=EINVAL; return 0; }
    size_t padded=(n+3)&~(size_t)3;
    if(6+padded/4>c->max_request) { errno=EMSGSIZE; return 0; }
    unsigned char *p=(unsigned char *)calloc(20+padded,1);
    if(!p)return 0;
    mx11_put32(p,w); mx11_put32(p+4,property); mx11_put32(p+8,type);
    p[12]=format; mx11_put32(p+16,(uint32_t)(n/unit));
    if(n)memcpy(p+20,data,n);
    int ok=mulib_x11_wire_request(c,18,0,p,20+padded);
    free(p);
    return ok;
}
static inline int mulib_x11_wm_title(mx11_connection *c,uint32_t w,const char *title) {
    return title&&mulib_x11_wm_property(c,w,39,31,8,title,strlen(title));
}
#endif
