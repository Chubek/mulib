/* MuLIB graphical extension: immgtk-hexeditor. C11, header-only. */
#ifndef MULIB_IMMGTK_HEXEDITOR_H
#define MULIB_IMMGTK_HEXEDITOR_H
#include "mulib-immgtk-native.h"
static inline int mulib_immgtk_hexeditor_set(unsigned char *data,size_t n,size_t offset,unsigned nibble,int high){if(!data||offset>=n||nibble>15)return 0;data[offset]=high?(data[offset]&15)|(nibble<<4):(data[offset]&240)|nibble;return 1;}
static inline void mulib_immgtk_hexeditor_draw(mulib_immgtk_native_t *g,const unsigned char *p,size_t n,size_t first,mgx_rect r){for(size_t i=first;i<n&&i-first<(size_t)mgx_max(0,r.h/20)*16;){char line[100];int k=snprintf(line,sizeof line,"%08zx  ",i);size_t row=i/16;for(unsigned j=0;j<16&&i<n;j++,i++)k+=snprintf(line+k,sizeof line-(size_t)k,"%02x ",p[i]);mig_label(g,(mgx_rect){r.x,r.y+(int)(row-first/16)*20,r.w,20},line);}}
#endif
