/* MuLIB graphical extension: immgtk-viewport. C11, header-only. */
#ifndef MULIB_IMMGTK_VIEWPORT_H
#define MULIB_IMMGTK_VIEWPORT_H
#include "mulib-immgtk-native.h"
#include "mulib-glx-window.h"
typedef struct{uint64_t id;mgx_rect bounds;double dpi;mulib_glx_window_t *native;}mig_viewport;
static inline int mulib_immgtk_viewport_at(const mig_viewport *v,size_t n,double x,double y){for(size_t i=n;i;i--)if(mgx_contains(v[i-1].bounds,x,y))return (int)(i-1);return -1;}
static inline mgx_point mulib_immgtk_viewport_local(const mig_viewport *v,double x,double y){double s=v->dpi>0?v->dpi:1;return (mgx_point){(x-v->bounds.x)/s,(y-v->bounds.y)/s};}
#endif
