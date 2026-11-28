/* MuLIB browser staging addition: web-canvas2d
 * Implemented subset: Checked RGBA8 rectangular fill and pixel read on caller-owned canvas.
 * Remaining: Paths, transforms, compositing, fonts, origin-clean enforcement and JS API.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_CANVAS2D_H
#define MULIB_WEB_CANVAS2D_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {unsigned char *rgba;size_t width,height,stride,bytes;} mulib_web_canvas2d_t;
static inline int mulib_web_canvas2d_valid(const mulib_web_canvas2d_t *c){size_t row,total;return c&&muba_mul_size(c->width,4,&row)&&c->stride>=row&&muba_mul_size(c->height,c->stride,&total)&&total<=c->bytes&&(c->rgba||!total);}
static inline int mulib_web_canvas2d_fill(mulib_web_canvas2d_t *c,size_t x,size_t y,size_t w,size_t h,const unsigned char rgba[4]){if(!mulib_web_canvas2d_valid(c)||!rgba||x>c->width||y>c->height||w>c->width-x||h>c->height-y)return 0;unsigned char color[4];memcpy(color,rgba,4);for(size_t j=0;j<h;j++)for(size_t i=0;i<w;i++)memcpy(c->rgba+(y+j)*c->stride+(x+i)*4,color,4);return 1;}
static inline int mulib_web_canvas2d_pixel(const mulib_web_canvas2d_t *c,size_t x,size_t y,unsigned char out[4]){if(!mulib_web_canvas2d_valid(c)||!out||x>=c->width||y>=c->height)return 0;memmove(out,c->rgba+y*c->stride+x*4,4);return 1;}
#endif /* MULIB_WEB_CANVAS2D_H */
