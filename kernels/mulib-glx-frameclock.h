/* MuLIB notebook desktop extension. C11, header-only.
 * SPDX-License-Identifier: MIT
 * See AGENTS.md for integration and coverage limits. */
#ifndef MULIB_GLX_FRAMECLOCK_H
#define MULIB_GLX_FRAMECLOCK_H
#include "mulib-wm-core.h"
/* Caller monotonic nanoseconds. At most one outstanding frame. */
typedef struct {uint64_t interval,next,submitted,last_present;int dirty,busy;} mulib_glx_frameclock;
static inline int mulib_glx_frameclock_init(mulib_glx_frameclock *c,unsigned millihz,uint64_t now){if(!c||!millihz)return 0;*c=(mulib_glx_frameclock){.interval=UINT64_C(1000000000000)/millihz,.next=now};return c->interval!=0;}
static inline void mulib_glx_frameclock_damage(mulib_glx_frameclock *c){if(c)c->dirty=1;}
static inline int mulib_glx_frameclock_begin(mulib_glx_frameclock *c,uint64_t now){if(!c||c->busy||!c->dirty||now<c->next)return 0;c->busy=1;c->dirty=0;c->submitted=now;return 1;}
static inline int mulib_glx_frameclock_done(mulib_glx_frameclock *c,uint64_t now,int success){if(!c||!c->busy||now<c->submitted)return 0;c->busy=0;if(success)c->last_present=now;else c->dirty=1;c->next=now>UINT64_MAX-c->interval?UINT64_MAX:now+c->interval;return 1;}
#endif
