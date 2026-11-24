/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_DATADEVICE_H
#define MULIB_WL_DATADEVICE_H
#include "mulib-wm-core.h"
typedef struct {uint64_t source,focus;uint32_t serial;char **mime;size_t n,cap;} mulib_wl_dataoffer;
static inline int mulib_wl_dataoffer_add(mulib_wl_dataoffer *o,const char *mime){if(!o||!mime||!*mime||strlen(mime)>255||o->n>=128)return 0;for(size_t i=0;i<o->n;i++)if(!strcmp(o->mime[i],mime))return 1;char *s=mulib_wm_strdup(mime);if(!s||!mulib_wm_grow((void**)&o->mime,&o->cap,o->n+1,sizeof s)){free(s);return 0;}o->mime[o->n++]=s;return 1;}
static inline int mulib_wl_dataoffer_accept(const mulib_wl_dataoffer *o,uint64_t focus,uint32_t serial,const char *mime){if(!o||!focus||focus!=o->focus||!serial||serial!=o->serial||!mime)return 0;for(size_t i=0;i<o->n;i++)if(!strcmp(o->mime[i],mime))return 1;return 0;}
static inline void mulib_wl_dataoffer_free(mulib_wl_dataoffer *o){if(o){for(size_t i=0;i<o->n;i++)free(o->mime[i]);free(o->mime);memset(o,0,sizeof *o);}}
#endif
