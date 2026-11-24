/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WM_KEYBIND_H
#define MULIB_WM_KEYBIND_H
#include "mulib-wm-core.h"
typedef struct {uint32_t key,mods,mode;uint64_t command;} mulib_wm_binding;
typedef struct {mulib_wm_binding *v;size_t n,cap;uint32_t mode,ignored_mods;} mulib_wm_keybind;
static inline int mulib_wm_keybind_set(mulib_wm_keybind *b,mulib_wm_binding v){if(!b||!v.key||!v.command||b->n==SIZE_MAX)return 0;v.mods&=~b->ignored_mods;for(size_t i=0;i<b->n;i++)if(b->v[i].key==v.key&&b->v[i].mods==v.mods&&b->v[i].mode==v.mode){b->v[i]=v;return 1;}if(!mulib_wm_grow((void**)&b->v,&b->cap,b->n+1,sizeof *b->v))return 0;b->v[b->n++]=v;return 1;}
static inline uint64_t mulib_wm_keybind_get(const mulib_wm_keybind *b,uint32_t key,uint32_t mods){if(b)for(size_t i=0;i<b->n;i++)if(b->v[i].key==key&&b->v[i].mode==b->mode&&b->v[i].mods==(mods&~b->ignored_mods))return b->v[i].command;return 0;}
static inline void mulib_wm_keybind_free(mulib_wm_keybind *b){if(b){free(b->v);memset(b,0,sizeof *b);}}
#endif
