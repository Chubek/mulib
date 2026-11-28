/* MuLIB browser staging addition: browser-renderer
 * Implemented subset: Capability negotiation and versioned event provider attachment with reentry guard.
 * Remaining: Renderer synchronization, geometry, resources, input, failure recovery and security.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_RENDERER_H
#define MULIB_BROWSER_RENDERER_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-browser-wire.h"
typedef int (*mulib_browser_renderer_emit_fn)(void *,const mulib_browser_wire_event *);
typedef struct {uint64_t caps;unsigned version;mulib_browser_renderer_emit_fn emit;void *ctx;int busy;} mulib_browser_renderer_t;
static inline int mulib_browser_renderer_attach(mulib_browser_renderer_t *r,unsigned version,uint64_t offered,uint64_t required,mulib_browser_renderer_emit_fn emit,void *ctx){if(!r||r->emit||version!=1||!emit||(offered&required)!=required)return 0;*r=(mulib_browser_renderer_t){offered,version,emit,ctx,0};return 1;}
static inline int mulib_browser_renderer_publish(mulib_browser_renderer_t *r,const mulib_browser_wire_event *e){if(!r||!r->emit||r->busy||!e||!e->seq||!muba_span_ok(e->payload))return 0;r->busy=1;int ok=r->emit(r->ctx,e);r->busy=0;return ok;}
#endif /* MULIB_BROWSER_RENDERER_H */
