/* MuLIB graphical extension: configured X11 PutImage requests. C11. */
#ifndef MULIB_X11_IMAGE_H
#define MULIB_X11_IMAGE_H
#include "mulib-x11-wire.h"
/* Caller must verify drawable visual/channel masks separately. This checks a
 * matching setup pixmap format and LSBFirst image byte order, not a visual. */
static inline int mulib_x11_image_setup_format(const unsigned char *p,size_t n,uint8_t depth) {
    if(!mulib_x11_wire_setup_body_valid(p,n)||p[22]!=0||(depth!=24&&depth!=32))return 0;
    size_t pos=32+(((size_t)mx11_get16(p+16)+3)&~(size_t)3);
    for(unsigned i=0;i<p[21];i++,pos+=8)
        if(p[pos]==depth&&p[pos+1]==32&&p[pos+2]==32)return 1;
    return 0;
}
/* ZPixmap, 32bpp little-endian XRGB/ARGB only; no TrueColor negotiation,
 * alpha compositing, XRender or server completion is implied. */
static inline int mulib_x11_image_put(mx11_connection *c,uint32_t drawable,uint32_t gc,
        const mgx_surface *s,uint8_t depth) {
    if(!c||c->failed||!drawable||!gc||!s||!s->pixels||s->w<=0||s->h<=0||
       s->w>32767||s->h>32767||s->stride<(size_t)s->w||
       s->stride>SIZE_MAX/sizeof(uint32_t)/(size_t)s->h||
       (depth!=24&&depth!=32)||c->max_request<=6) { errno=EINVAL; return 0; }
    size_t max=(size_t)c->max_request*4-24;
    size_t row=(size_t)s->w*4;
    if(row>max) { errno=EMSGSIZE; return 0; }
    size_t rows=max/row;
    for(int y=0;y<s->h;) {
        size_t nr=(size_t)(s->h-y)<rows?(size_t)(s->h-y):rows;
        size_t bytes=nr*row;
        unsigned char *p=(unsigned char *)calloc(20+bytes,1);
        if(!p)return 0;
        mx11_put32(p,drawable); mx11_put32(p+4,gc);
        mx11_put16(p+8,(uint16_t)s->w); mx11_put16(p+10,(uint16_t)nr);
        mx11_put16(p+14,(uint16_t)y); p[17]=depth;
        for(size_t j=0;j<nr;j++)for(int x=0;x<s->w;x++)
            mx11_put32(p+20+j*row+(size_t)x*4,
                       s->pixels[((size_t)y+j)*s->stride+(size_t)x]);
        int ok=mulib_x11_wire_request(c,72,2,p,20+bytes);
        free(p);
        if(!ok)return 0;
        y+=(int)nr;
    }
    return 1;
}
#endif
