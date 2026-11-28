/* MuLIB browser staging addition: web-offscreen-canvas
 * Implemented subset: Explicit offscreen canvas ownership token transfer.
 * Remaining: Rendering contexts, transfer-to-image-bitmap and worker bindings.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_OFFSCREEN_CANVAS_H
#define MULIB_WEB_OFFSCREEN_CANVAS_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t owner;uintptr_t canvas;} mulib_web_offscreen_canvas_t;
static inline int mulib_web_offscreen_canvas_transfer(mulib_web_offscreen_canvas_t *c,uint64_t from,uint64_t to){if(!c||!c->canvas||!to||!from||c->owner!=from)return 0;c->owner=to;return 1;}
#endif /* MULIB_WEB_OFFSCREEN_CANVAS_H */
