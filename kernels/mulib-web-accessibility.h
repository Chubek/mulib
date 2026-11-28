/* MuLIB browser staging addition: web-accessibility
 * Implemented subset: Basic accessible-name priority from supplied aria-label, label and text.
 * Remaining: Accessible name computation, roles, tree, relations and platform APIs.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_ACCESSIBILITY_H
#define MULIB_WEB_ACCESSIBILITY_H
#include "detail/mulib-browser-additions-common.h"

static inline muba_span mulib_web_accessibility_name(muba_span aria,muba_span label,muba_span text){aria=muba_trim(aria);label=muba_trim(label);return aria.len?aria:label.len?label:muba_trim(text);}
#endif /* MULIB_WEB_ACCESSIBILITY_H */
