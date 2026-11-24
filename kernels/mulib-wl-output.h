/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_OUTPUT_H
#define MULIB_WL_OUTPUT_H
#include "mulib-wl-client.h"
/* wl_output.mode, scale, done event helpers for an already bound resource. */
static inline int mulib_wl_output_mode(mulib_wl_client *c,uint32_t id,uint32_t flags,int32_t w,int32_t h,int32_t refresh){if(w<=0||h<=0||refresh<0)return 0;uint32_t p[4]={flags,(uint32_t)w,(uint32_t)h,(uint32_t)refresh};return mulib_wl_client_event(c,id,1,p,16);}
static inline int mulib_wl_output_scale(mulib_wl_client *c,uint32_t id,int32_t factor){mulib_wl_resource *r=c?mulib_wl_resource_find(&c->objects,id):NULL;return r&&r->version>=2&&factor>0&&mulib_wl_client_event(c,id,3,&factor,4);}
static inline int mulib_wl_output_done(mulib_wl_client *c,uint32_t id){mulib_wl_resource *r=c?mulib_wl_resource_find(&c->objects,id):NULL;return r&&r->version>=2&&mulib_wl_client_event(c,id,2,NULL,0);}
#endif
