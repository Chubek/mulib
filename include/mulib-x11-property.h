/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_X11_PROPERTY_H
#define MULIB_X11_PROPERTY_H
#include "mulib-x11-ext.h"
typedef struct {uint32_t type,items,remaining;uint8_t format;unsigned char *data;size_t bytes;} mulib_x11_property;
static inline int mulib_x11_property_get(mulib_x11_wm_io *io,uint32_t window,uint32_t atom,uint32_t type,uint32_t offset,uint32_t words,int remove,mulib_x11_property *out){if(!window||!atom||!out||words>1048576)return 0;unsigned char p[20],*r;size_t n;mulib_x11_wm_u32(p,window);mulib_x11_wm_u32(p+4,atom);mulib_x11_wm_u32(p+8,type);mulib_x11_wm_u32(p+12,offset);mulib_x11_wm_u32(p+16,words);if(!mulib_x11_wm_reply(io,20,!!remove,p,20,&r,&n))return 0;uint8_t f=r[1];uint32_t count=mulib_x11_wm_r32(r+16);uint64_t bytes=(uint64_t)count*(f/8);if((f!=0&&f!=8&&f!=16&&f!=32)||(!f&&count)||bytes>n-32||bytes>(uint64_t)words*4){free(r);return 0;}unsigned char *d=bytes?(unsigned char*)malloc((size_t)bytes):NULL;if(bytes&&!d){free(r);return 0;}if(bytes)memcpy(d,r+32,(size_t)bytes);*out=(mulib_x11_property){mulib_x11_wm_r32(r+8),count,mulib_x11_wm_r32(r+12),f,d,(size_t)bytes};free(r);return 1;}
static inline int mulib_x11_property_u32(const mulib_x11_property *p,size_t i,uint32_t *out){if(!p||!out||p->format!=32||i>=p->items||i>=p->bytes/4)return 0;*out=mulib_x11_wm_r32(p->data+i*4);return 1;}
static inline int mulib_x11_property_set(mulib_x11_wm_io *io,uint32_t w,uint32_t atom,uint32_t type,uint8_t format,const void *wire,size_t bytes){size_t u=format/8;if(!w||!atom||!type||(format!=8&&format!=16&&format!=32)||bytes%u||bytes>262100||(!wire&&bytes))return 0;size_t n=20+(bytes+3)/4*4;unsigned char *p=(unsigned char*)calloc(n,1);if(!p)return 0;mulib_x11_wm_u32(p,w);mulib_x11_wm_u32(p+4,atom);mulib_x11_wm_u32(p+8,type);p[12]=format;mulib_x11_wm_u32(p+16,(uint32_t)(bytes/u));if(bytes)memcpy(p+20,wire,bytes);int ok=mulib_x11_wm_send(io,18,0,p,n);free(p);return ok;}
static inline void mulib_x11_property_free(mulib_x11_property *p){if(p){free(p->data);memset(p,0,sizeof *p);}}
#endif
