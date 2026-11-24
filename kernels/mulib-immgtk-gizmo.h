/* MuLIB graphical extension: immgtk-gizmo. C11, header-only. */
#ifndef MULIB_IMMGTK_GIZMO_H
#define MULIB_IMMGTK_GIZMO_H
#include "mulib-immgtk-native.h"
static inline int mulib_immgtk_gizmo_translate(mulib_immgtk_native_t *g,const char *id,mgx_point *p,double snap){if(!g||!id||!p||!isfinite(p->x+p->y+snap))return 0;mgx_rect r={mgx_int(p->x-8.0),mgx_int(p->y-8.0),16,16};uint64_t k=mig_id(g,id);mig_interact(g,k,r,1);mig_rect(g,r,g->theme.accent);if(g->active!=k||!g->down)return 0;mgx_point q={g->x,g->y};if(snap>0){q.x=round(q.x/snap)*snap;q.y=round(q.y/snap)*snap;}if(!isfinite(q.x)||!isfinite(q.y))return 0;int c=q.x!=p->x||q.y!=p->y;*p=q;return c;}
#endif
