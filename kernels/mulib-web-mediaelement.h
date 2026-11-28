/* MuLIB browser staging addition: web-mediaelement
 * Implemented subset: Caller-clocked playback position advance and end detection.
 * Remaining: Network, decoding, buffering, seek, events and autoplay policy.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_MEDIAELEMENT_H
#define MULIB_WEB_MEDIAELEMENT_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {double current,duration,rate;int paused,ended;} mulib_web_mediaelement_t;
static inline int mulib_web_mediaelement_tick(mulib_web_mediaelement_t *m,double delta){if(!m||!isfinite(delta)||delta<0||!isfinite(m->current)||!isfinite(m->duration)||!isfinite(m->rate)||m->duration<0||m->rate<0)return 0;if(!m->paused&&!m->ended){m->current=fmin(m->duration,m->current+delta*m->rate);if(m->current>=m->duration)m->ended=1;}return 1;}
#endif /* MULIB_WEB_MEDIAELEMENT_H */
