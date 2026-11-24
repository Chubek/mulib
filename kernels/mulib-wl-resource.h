/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_RESOURCE_H
#define MULIB_WL_RESOURCE_H
#include "mulib-wm-core.h"
struct mulib_wl_client;struct mulib_wl_resource;
typedef int (*mulib_wl_request_fn)(struct mulib_wl_client *,struct mulib_wl_resource *,uint16_t,const unsigned char *,size_t);
typedef void (*mulib_wl_destroy_fn)(void *);
typedef struct mulib_wl_resource {uint32_t id,version;const char *interface;mulib_wl_request_fn request;void *user;mulib_wl_destroy_fn destroy;} mulib_wl_resource;
typedef struct {mulib_wl_resource *v;size_t n,cap,limit;} mulib_wl_resources;
static inline mulib_wl_resource *mulib_wl_resource_find(mulib_wl_resources *r,uint32_t id){if(r&&id)for(size_t i=0;i<r->n;i++)if(r->v[i].id==id)return r->v+i;return NULL;}
static inline int mulib_wl_resource_add(mulib_wl_resources *r,mulib_wl_resource v){if(!r||!v.id||!v.version||!v.interface||!v.request||r->n>=r->limit||r->n==SIZE_MAX||mulib_wl_resource_find(r,v.id)||!mulib_wm_grow((void**)&r->v,&r->cap,r->n+1,sizeof v))return 0;r->v[r->n++]=v;return 1;}
static inline int mulib_wl_resource_remove(mulib_wl_resources *r,uint32_t id){mulib_wl_resource *v=mulib_wl_resource_find(r,id);if(!v)return 0;mulib_wl_resource old=*v;size_t i=(size_t)(v-r->v);memmove(v,v+1,(--r->n-i)*sizeof *v);if(old.destroy)old.destroy(old.user);return 1;}
static inline void mulib_wl_resources_free(mulib_wl_resources *r){if(r){while(r->n)mulib_wl_resource_remove(r,r->v[r->n-1].id);free(r->v);memset(r,0,sizeof *r);}}
#endif
