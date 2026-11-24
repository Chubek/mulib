/* MuLIB graphical extension: gui-hitest. C11, header-only. */
#ifndef MULIB_GUI_HITEST_H
#define MULIB_GUI_HITEST_H
#include "mulib-gui-core.h"
typedef struct{uint64_t id;mgx_rect rect,clip;int z,enabled;}mgui_hit;
static inline uint64_t mulib_gui_hitest(const mgui_hit *v,size_t n,double x,double y){uint64_t id=0;int z=INT_MIN;for(size_t i=0;i<n;i++)if(v[i].enabled&&v[i].z>=z&&mgx_contains(v[i].rect,x,y)&&mgx_contains(v[i].clip,x,y)){id=v[i].id;z=v[i].z;}return id;}
#endif
