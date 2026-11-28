/* MuLIB browser staging addition: browser-scriptload
 * Implemented subset: Recognize classic JavaScript MIME types and module type.
 * Remaining: Fetch, script preparation, MIME sniffing, integrity and execution.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_BROWSER_SCRIPTLOAD_H
#define MULIB_BROWSER_SCRIPTLOAD_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_browser_scriptload_type(muba_span type){type=muba_trim(type);if(!type.len)return 1;if(muba_ieq(type,muba_str("module")))return 2;const char *v[]={"text/javascript","application/javascript","text/ecmascript","application/ecmascript"};for(size_t i=0;i<4;i++)if(muba_ieq(type,muba_str(v[i])))return 1;return 0;}
#endif /* MULIB_BROWSER_SCRIPTLOAD_H */
