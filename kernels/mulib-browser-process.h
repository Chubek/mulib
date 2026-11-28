/* MuLIB browser staging addition: browser-process
 * Implemented subset: Explicit process spawn/stop provider with lifecycle and capability token.
 * Remaining: Real OS sandbox, process monitoring, crash handling and IPC.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_PROCESS_H
#define MULIB_BROWSER_PROCESS_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_browser_process_spawn_fn)(void *,uint64_t,uint64_t *);typedef int (*mulib_browser_process_stop_fn)(void *,uint64_t);
typedef struct {uint64_t process;void *ctx;mulib_browser_process_spawn_fn spawn;mulib_browser_process_stop_fn stop;} mulib_browser_process_t;
static inline int mulib_browser_process_start(mulib_browser_process_t *p,uint64_t capability){if(!p||p->process||!capability||!p->spawn)return 0;uint64_t id=0;if(!p->spawn(p->ctx,capability,&id)||!id)return 0;p->process=id;return 1;}
static inline int mulib_browser_process_terminate(mulib_browser_process_t *p){if(!p||!p->process||!p->stop||!p->stop(p->ctx,p->process))return 0;p->process=0;return 1;}
#endif /* MULIB_BROWSER_PROCESS_H */
