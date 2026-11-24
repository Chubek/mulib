/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_WL_SURFACE_H
#define MULIB_WL_SURFACE_H
#include "mulib-wl-buffer.h"
#include "mulib-glx-damage.h"
typedef enum {MULIB_WL_ROLE_NONE,MULIB_WL_ROLE_TOPLEVEL,MULIB_WL_ROLE_POPUP,MULIB_WL_ROLE_SUBSURFACE,MULIB_WL_ROLE_LAYER} mulib_wl_role;
typedef struct {mulib_wl_buffer *buffer;int scale,transform;int32_t dx,dy;} mulib_wl_surface_state;
typedef struct {uint64_t id,revision;mulib_wl_role role;mulib_wl_surface_state current,pending;int attach,configured;mulib_glx_damage damage;} mulib_wl_surface;
static inline void mulib_wl_surface_init(mulib_wl_surface *s,uint64_t id){*s=(mulib_wl_surface){.id=id,.current={.scale=1},.pending={.scale=1}};s->damage.limit=256;}
static inline int mulib_wl_surface_role(mulib_wl_surface *s,mulib_wl_role role){if(!s||role<=0||role>MULIB_WL_ROLE_LAYER||(s->role&&s->role!=role))return 0;s->role=role;return 1;}
static inline int mulib_wl_surface_attach(mulib_wl_surface *s,mulib_wl_buffer *b,int32_t x,int32_t y){if(!s||(b&&!mulib_wl_buffer_acquire(b)))return 0;if(s->attach)mulib_wl_buffer_release(s->pending.buffer);s->pending.buffer=b;s->pending.dx=x;s->pending.dy=y;s->attach=1;return 1;}
static inline int mulib_wl_surface_scale(mulib_wl_surface *s,int scale,int transform){if(!s||scale<=0||transform<0||transform>7)return 0;s->pending.scale=scale;s->pending.transform=transform;return 1;}
static inline int mulib_wl_surface_commit(mulib_wl_surface *s){if(!s)return 0;mulib_wl_buffer *b=s->attach?s->pending.buffer:s->current.buffer;if(b&&((s->role==MULIB_WL_ROLE_TOPLEVEL||s->role==MULIB_WL_ROLE_POPUP||s->role==MULIB_WL_ROLE_LAYER)&&!s->configured))return 0;if(b&&(b->image.w%s->pending.scale||b->image.h%s->pending.scale))return 0;if(s->attach){mulib_wl_buffer_release(s->current.buffer);s->current.buffer=b;s->pending.buffer=NULL;s->attach=0;}s->current.scale=s->pending.scale;s->current.transform=s->pending.transform;s->current.dx=s->pending.dx;s->current.dy=s->pending.dy;s->revision++;s->damage.full=1;return 1;}
static inline void mulib_wl_surface_free(mulib_wl_surface *s){if(s){if(s->attach)mulib_wl_buffer_release(s->pending.buffer);mulib_wl_buffer_release(s->current.buffer);mulib_glx_damage_free(&s->damage);memset(s,0,sizeof *s);}}
#endif
