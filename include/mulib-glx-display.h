/* MuLIB graphical extension: glx-display. C11, header-only. */
#ifndef MULIB_GLX_DISPLAY_H
#define MULIB_GLX_DISPLAY_H
#include "mulib-glx-core.h"
typedef struct{uint32_t id;mgx_rect bounds;double dpi,refresh;char name[64];}mulib_glx_display_t;
static inline double mulib_glx_display_scale(const mulib_glx_display_t *d){return d&&d->dpi>0?mgx_clamp(d->dpi/96.,.5,8):1;}
static inline int mulib_glx_display_at(const mulib_glx_display_t *d,size_t n,double x,double y){for(size_t i=0;i<n;i++)if(mgx_contains(d[i].bounds,x,y))return (int)i;return -1;}
#endif
