/* MuLIB browser staging addition: microjs-generator
 * Implemented subset: Generator lifecycle transitions with resumable host callback.
 * Remaining: Suspended interpreter stacks, yield delegation, throw/return.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_GENERATOR_H
#define MULIB_MICROJS_GENERATOR_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_microjs_generator_START,mulib_microjs_generator_RUNNING,mulib_microjs_generator_SUSPENDED,mulib_microjs_generator_DONE} mulib_microjs_generator_state;
typedef int (*mulib_microjs_generator_resume_fn)(void *,uintptr_t,uintptr_t *);
typedef struct {mulib_microjs_generator_state state;mulib_microjs_generator_resume_fn resume;void *ctx;} mulib_microjs_generator_t;
static inline int mulib_microjs_generator_next(mulib_microjs_generator_t *g,uintptr_t in,uintptr_t *out){if(!g||!out||!g->resume||g->state==mulib_microjs_generator_RUNNING)return -1;if(g->state==mulib_microjs_generator_DONE)return 0;g->state=mulib_microjs_generator_RUNNING;int r=g->resume(g->ctx,in,out);g->state=r>0?mulib_microjs_generator_SUSPENDED:mulib_microjs_generator_DONE;return r;}
#endif /* MULIB_MICROJS_GENERATOR_H */
