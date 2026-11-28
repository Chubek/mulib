/* MuLIB browser staging addition: web-xhr
 * Implemented subset: Ready-state transition checking.
 * Remaining: Network operations, response types, abort, timeout, events and CORS.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_XHR_H
#define MULIB_WEB_XHR_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_web_xhr_UNSENT,mulib_web_xhr_OPENED,mulib_web_xhr_HEADERS,mulib_web_xhr_LOADING,mulib_web_xhr_DONE} mulib_web_xhr_state;
static inline int mulib_web_xhr_transition(mulib_web_xhr_state *s,mulib_web_xhr_state next){if(!s||next<mulib_web_xhr_UNSENT||next>mulib_web_xhr_DONE)return 0;if(next==mulib_web_xhr_UNSENT||next==mulib_web_xhr_OPENED||(*s>=mulib_web_xhr_OPENED&&next>*s)){*s=next;return 1;}return 0;}
#endif /* MULIB_WEB_XHR_H */
