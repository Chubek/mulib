/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_GLX_OUTPUTLAYOUT_H
#define MULIB_GLX_OUTPUTLAYOUT_H
#include "mulib-wm-core.h"
typedef struct {uint64_t id;mulib_wm_rect logical;double scale;unsigned transform;} mulib_glx_output;
typedef struct {mulib_glx_output *v;size_t n,cap;} mulib_glx_outputlayout;
static inline int mulib_glx_outputlayout_set(mulib_glx_outputlayout *l,mulib_glx_output v){if(!l||!v.id||!mulib_wm_rect_valid(v.logical)||!v.logical.w||!v.logical.h||!isfinite(v.scale)||v.scale<=0||v.transform>7)return 0;for(size_t i=0;i<l->n;i++)if(l->v[i].id==v.id){l->v[i]=v;return 1;}if(l->n==SIZE_MAX||!mulib_wm_grow((void**)&l->v,&l->cap,l->n+1,sizeof v))return 0;l->v[l->n++]=v;return 1;}
static inline uint64_t mulib_glx_outputlayout_at(const mulib_glx_outputlayout *l,double x,double y){if(l)for(size_t i=0;i<l->n;i++)if(mulib_wm_contains(l->v[i].logical,x,y))return l->v[i].id;return 0;}
static inline int mulib_glx_outputlayout_remove(mulib_glx_outputlayout *l,uint64_t id){if(l)for(size_t i=0;i<l->n;i++)if(l->v[i].id==id){memmove(l->v+i,l->v+i+1,(--l->n-i)*sizeof *l->v);return 1;}return 0;}
static inline void mulib_glx_outputlayout_free(mulib_glx_outputlayout *l){if(l){free(l->v);memset(l,0,sizeof *l);}}
#endif
