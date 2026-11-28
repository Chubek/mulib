/* MuLIB browser staging addition: web-taskqueue
 * Implemented subset: Round-robin bounded task-source scheduler.
 * Remaining: HTML runnable task criteria, microtask checkpoints, priorities and thread safety.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_TASKQUEUE_H
#define MULIB_WEB_TASKQUEUE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_queue *sources;size_t count,next;int active;} mulib_web_taskqueue_t;
static inline int mulib_web_taskqueue_step(mulib_web_taskqueue_t *s){if(!s||!s->count||!s->sources||s->active||s->next>=s->count)return 0;s->active=1;int r=0;for(size_t i=0;i<s->count;i++){size_t j=s->next;s->next=(j+1)%s->count;if(muba_queue_run(s->sources+j,1)){r=1;break;}}s->active=0;return r;}
#endif /* MULIB_WEB_TASKQUEUE_H */
