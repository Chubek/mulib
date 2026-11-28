/* MuLIB browser staging addition: web-agentcluster
 * Implemented subset: Shared-memory eligibility predicate using cluster IDs and isolation state.
 * Remaining: Agent construction, cross-origin isolation and memory model integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_AGENTCLUSTER_H
#define MULIB_WEB_AGENTCLUSTER_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_agentcluster_shared(uint64_t a,uint64_t b,int isolated_a,int isolated_b){return a&&a==b&&isolated_a&&isolated_b;}
#endif /* MULIB_WEB_AGENTCLUSTER_H */
