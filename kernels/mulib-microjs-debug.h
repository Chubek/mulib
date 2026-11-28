/* MuLIB browser staging addition: microjs-debug
 * Implemented subset: Bounded stack trace push/pop and top-frame access.
 * Remaining: Engine instrumentation, debugger breakpoints, source maps and pause protocol.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_DEBUG_H
#define MULIB_MICROJS_DEBUG_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t script;size_t pc,line,column;} mulib_microjs_debug_frame;
typedef struct {mulib_microjs_debug_frame *frames;size_t len,cap;} mulib_microjs_debug_stack;
static inline int mulib_microjs_debug_push(mulib_microjs_debug_stack *s,mulib_microjs_debug_frame f){if(!s||s->len>=s->cap)return 0;s->frames[s->len++]=f;return 1;}
static inline int mulib_microjs_debug_pop(mulib_microjs_debug_stack *s,mulib_microjs_debug_frame *f){if(!s||!f||!s->len)return 0;*f=s->frames[--s->len];return 1;}
#endif /* MULIB_MICROJS_DEBUG_H */
