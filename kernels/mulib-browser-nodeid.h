/* MuLIB browser staging addition: browser-nodeid
 * Implemented subset: Monotonic nonzero node IDs with overflow prevention.
 * Remaining: Per-document allocation, generation IDs and DOM lifetime mapping.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_NODEID_H
#define MULIB_BROWSER_NODEID_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {uint64_t next;} mulib_browser_nodeid_t;
static inline int mulib_browser_nodeid_next(mulib_browser_nodeid_t *s,uint64_t *id){if(!s||!id||s->next==UINT64_MAX)return 0;*id=++s->next;return 1;}
#endif /* MULIB_BROWSER_NODEID_H */
