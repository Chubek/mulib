/* MuLIB browser staging addition: microjs-agent
 * Implemented subset: Agent identity and cluster equality.
 * Remaining: Threaded agents, scheduling, memory model and VM execution.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_MICROJS_AGENT_H
#define MULIB_MICROJS_AGENT_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t id,cluster;int can_block;} mulib_microjs_agent_t;
static inline int mulib_microjs_agent_share(const mulib_microjs_agent_t *a,const mulib_microjs_agent_t *b){return a&&b&&a->id&&b->id&&a->cluster&&a->cluster==b->cluster;}
#endif /* MULIB_MICROJS_AGENT_H */
