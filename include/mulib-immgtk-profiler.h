/* MuLIB graphical extension: immgtk-profiler. C11, header-only. */
#ifndef MULIB_IMMGTK_PROFILER_H
#define MULIB_IMMGTK_PROFILER_H
#include "mulib-immgtk-native.h"
typedef struct{const char *name;double start,end;unsigned depth;}mig_profile_span;
static inline void mulib_immgtk_profiler_draw(mulib_immgtk_native_t *g,const mig_profile_span *v,size_t n,double start,double end,mgx_rect r){if(end<=start)return;mgx_rect old=g->clip;g->clip=mgx_intersect(old,r);for(size_t i=0;i<n;i++){int x=mgx_int(r.x+(v[i].start-start)/(end-start)*r.w),w=mgx_int((v[i].end-v[i].start)/(end-start)*r.w);mgx_rect a={x,mgx_int(r.y+v[i].depth*20.),w,18};mig_rect(g,a,0xff000000|(uint32_t)(mgx_hash(v[i].name,strlen(v[i].name),0)&0x7f7f7f)|0x505050);mig_label(g,a,v[i].name);}g->clip=old;}
#endif
