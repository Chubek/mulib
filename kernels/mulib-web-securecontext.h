/* MuLIB browser staging addition: web-securecontext
 * Implemented subset: Conservative HTTPS plus secure ancestor predicate.
 * Remaining: Potentially trustworthy origins, localhost, file and inherited contexts.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_SECURECONTEXT_H
#define MULIB_WEB_SECURECONTEXT_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_securecontext_secure(muba_span scheme,int secure_ancestors){return secure_ancestors&&muba_ieq(scheme,muba_str("https"));}
#endif /* MULIB_WEB_SECURECONTEXT_H */
