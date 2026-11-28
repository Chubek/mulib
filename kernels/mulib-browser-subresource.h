/* MuLIB browser staging addition: browser-subresource
 * Implemented subset: Classify common tag/attribute pairs into resource destinations.
 * Remaining: Discovery from CSS/srcset/modules, scheduling, policies and network.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_SUBRESOURCE_H
#define MULIB_BROWSER_SUBRESOURCE_H
#include "detail/mulib-browser-additions-common.h"

static inline const char *mulib_browser_subresource_destination(muba_span tag,muba_span attr){if(muba_ieq(attr,muba_str("src"))){if(muba_ieq(tag,muba_str("script")))return "script";if(muba_ieq(tag,muba_str("img")))return "image";if(muba_ieq(tag,muba_str("iframe")))return "document";if(muba_ieq(tag,muba_str("audio")))return "audio";if(muba_ieq(tag,muba_str("video")))return "video";}if(muba_ieq(tag,muba_str("link"))&&muba_ieq(attr,muba_str("href")))return "link";return NULL;}
#endif /* MULIB_BROWSER_SUBRESOURCE_H */
