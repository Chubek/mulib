/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_REGISTRY_H
#define MULIB_WL_REGISTRY_H
#include "mulib-wl-dispatch.h"
typedef int (*mulib_wl_bind_fn)(mulib_wl_client *,uint32_t,uint32_t,void *);
typedef struct {uint32_t name,version;const char *interface;mulib_wl_bind_fn bind;void *user;} mulib_wl_global;
typedef struct {mulib_wl_global *v;size_t n,cap;uint32_t next;} mulib_wl_registry;
static inline uint32_t mulib_wl_registry_add(mulib_wl_registry *r,const char *iface,uint32_t version,mulib_wl_bind_fn bind,void *u){if(!r||!iface||!*iface||strlen(iface)>255||!version||!bind||r->next==UINT32_MAX||r->n==SIZE_MAX||!mulib_wm_grow((void**)&r->v,&r->cap,r->n+1,sizeof *r->v))return 0;uint32_t name=++r->next;r->v[r->n++]=(mulib_wl_global){name,version,iface,bind,u};return name;}
static inline int mulib_wl_registry_announce(mulib_wl_client *c,uint32_t id,const mulib_wl_global *g){unsigned char p[268]={0};uint32_t z=(uint32_t)strlen(g->interface)+1;size_t pad=(z+3)&~(size_t)3;memcpy(p,&g->name,4);memcpy(p+4,&z,4);memcpy(p+8,g->interface,z);memcpy(p+8+pad,&g->version,4);return mulib_wl_client_event(c,id,0,p,12+pad);}
static inline int mulib_wl_registry_request(mulib_wl_client *c,mulib_wl_resource *res,uint16_t op,const unsigned char *p,size_t n){if(op||n<16)return 0;mulib_wl_registry *r=(mulib_wl_registry*)res->user;const char *iface;size_t used;if(!mulib_wl_string_read(p+4,n-4,&iface,&used)||n!=12+used)return 0;uint32_t name=mulib_wl_u32(p),version=mulib_wl_u32(p+4+used),id=mulib_wl_u32(p+8+used);if(id<2||id>=0xff000000u||mulib_wl_resource_find(&c->objects,id))return 0;for(size_t i=0;i<r->n;i++){mulib_wl_global *g=r->v+i;if(g->name==name&&version&&version<=g->version&&!strcmp(g->interface,iface))return g->bind(c,id,version,g->user);}return 0;}
static inline void mulib_wl_registry_free(mulib_wl_registry *r){if(r){free(r->v);memset(r,0,sizeof *r);}}
#endif
