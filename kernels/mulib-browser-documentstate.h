/* MuLIB browser staging addition: browser-documentstate
 * Implemented subset: Active/inactive/discarded lifecycle with irreversible discard.
 * Remaining: Fully active documents, BFCache, freezing and resource cleanup.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_DOCUMENTSTATE_H
#define MULIB_BROWSER_DOCUMENTSTATE_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_browser_documentstate_ACTIVE,mulib_browser_documentstate_INACTIVE,mulib_browser_documentstate_DISCARDED} mulib_browser_documentstate_state;
static inline int mulib_browser_documentstate_set(mulib_browser_documentstate_state *s,mulib_browser_documentstate_state next){if(!s||*s==mulib_browser_documentstate_DISCARDED||next<mulib_browser_documentstate_ACTIVE||next>mulib_browser_documentstate_DISCARDED)return 0;*s=next;return 1;}
#endif /* MULIB_BROWSER_DOCUMENTSTATE_H */
