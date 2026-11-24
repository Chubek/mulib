/* MuLIB graphical extension: glx-filter. C11, header-only. */
#ifndef MULIB_GLX_FILTER_H
#define MULIB_GLX_FILTER_H
#include "mulib-glx-core.h"
static inline int mulib_glx_filter_box(mgx_surface *dst,const mgx_surface *src,unsigned radius){if(!dst||!src||dst->pixels==src->pixels||dst->w!=src->w||dst->h!=src->h||radius>64)return 0;for(int y=0;y<src->h;y++)for(int x=0;x<src->w;x++){uint64_t sums[4]={0};unsigned n=0;for(int j=mgx_max(0,y-(int)radius);j<=mgx_min(src->h-1,y+(int)radius);j++)for(int i=mgx_max(0,x-(int)radius);i<=mgx_min(src->w-1,x+(int)radius);i++){uint32_t c=src->pixels[(size_t)j*src->stride+i];for(int k=0;k<4;k++)sums[k]+=mgx_ch(c,k*8);n++;}uint32_t c=0;for(int k=0;k<4;k++)c|=(uint32_t)(sums[k]/n)<<(8*k);dst->pixels[(size_t)y*dst->stride+x]=c;}return 1;}
#endif
