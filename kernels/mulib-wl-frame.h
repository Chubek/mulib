/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_FRAME_H
#define MULIB_WL_FRAME_H
#include "mulib-wl-dispatch.h"
typedef struct {uint32_t *v;size_t n,cap,limit;} mulib_wl_frames;
static inline int mulib_wl_frame_add(mulib_wl_frames *f,uint32_t callback){if(!f||callback<2||callback>=0xff000000u||f->n>=f->limit||f->n==SIZE_MAX)return 0;for(size_t i=0;i<f->n;i++)if(f->v[i]==callback)return 0;if(!mulib_wm_grow((void**)&f->v,&f->cap,f->n+1,sizeof callback))return 0;f->v[f->n++]=callback;return 1;}
/* Invoke only for callbacks committed with the presented surface; client owns
 * resource IDs. A partial event queue failure is terminal, never retry it. */
static inline int mulib_wl_frame_done(mulib_wl_frames *f,mulib_wl_client *c,uint32_t msec)
{
    if(!f||!c||c->failed)return 0;
    for(size_t i=0;i<f->n;i++) {
        uint32_t id=f->v[i];
        mulib_wl_resource *r=mulib_wl_resource_find(&c->objects,id);
        if(!r||strcmp(r->interface,"wl_callback")||
           !mulib_wl_client_event(c,id,0,&msec,4)||!mulib_wl_delete_id(c,id)||
           !mulib_wl_resource_remove(&c->objects,id)) {
            (void)mulib_wl_client_fail(c);
            return 0;
        }
    }
    f->n=0;
    return 1;
}
static inline void mulib_wl_frames_free(mulib_wl_frames *f){if(f){free(f->v);memset(f,0,sizeof *f);}}
#endif
