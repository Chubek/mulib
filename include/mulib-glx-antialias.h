/* MuLIB graphical extension: glx-antialias. C11, header-only. */
#ifndef MULIB_GLX_ANTIALIAS_H
#define MULIB_GLX_ANTIALIAS_H
#include "mulib-glx-raster.h"
static inline void mulib_glx_antialias_fill(mgx_surface *s,const mulib_glx_path_t *p,uint32_t c){mulib_glx_raster_fill(s,p,c,0,4);}
static inline uint8_t mulib_glx_antialias_mask(const mulib_glx_path_t *p,int x,int y){return (uint8_t)(255*mulib_glx_raster_coverage(p,x,y,0,4)+.5);}
#endif
