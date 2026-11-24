/* MuLIB graphical extension: glx-rendercache. C11, header-only. */
#ifndef MULIB_GLX_RENDERCACHE_H
#define MULIB_GLX_RENDERCACHE_H
#include "mulib-glx-core.h"
typedef struct{uint64_t key,stamp;mgx_surface image;}mgx_cache_entry;
typedef struct{mgx_cache_entry *v;size_t n,limit;uint64_t clock;}mulib_glx_rendercache_t;
static inline int mulib_glx_rendercache_init(mulib_glx_rendercache_t *c,size_t limit){if(!limit||limit>SIZE_MAX/sizeof *c->v)return 0;c->v=(mgx_cache_entry*)calloc(limit,sizeof *c->v);if(!c->v)return 0;c->n=0;c->limit=limit;c->clock=0;return 1;}
static inline mgx_surface *mulib_glx_rendercache_get(mulib_glx_rendercache_t *c,uint64_t key){for(size_t i=0;i<c->n;i++)if(c->v[i].key==key){c->v[i].stamp=++c->clock;return &c->v[i].image;}return NULL;}
static inline int mulib_glx_rendercache_put(mulib_glx_rendercache_t *c,uint64_t key,mgx_surface *owned){if(!c||!c->limit||!owned||!owned->owned)return 0;size_t j=0;for(size_t i=0;i<c->n;i++)if(c->v[i].key==key){j=i;goto put;}if(c->n<c->limit)j=c->n++;else for(size_t i=1;i<c->n;i++)if(c->v[i].stamp<c->v[j].stamp)j=i;put:mgx_surface_free(&c->v[j].image);c->v[j]=(mgx_cache_entry){key,++c->clock,*owned};memset(owned,0,sizeof *owned);return 1;}
static inline void mulib_glx_rendercache_free(mulib_glx_rendercache_t *c){for(size_t i=0;i<c->n;i++)mgx_surface_free(&c->v[i].image);free(c->v);memset(c,0,sizeof *c);}
#endif
