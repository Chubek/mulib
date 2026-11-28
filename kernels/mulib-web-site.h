/* MuLIB browser staging addition: web-site
 * Implemented subset: Schemeful-site equality on precomputed registrable domains.
 * Remaining: PSL and URL integration, opaque origins and IP handling.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_SITE_H
#define MULIB_WEB_SITE_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_web_site_same(muba_span a_scheme,muba_span a_domain,muba_span b_scheme,muba_span b_domain){return a_domain.len&&b_domain.len&&muba_ieq(a_scheme,b_scheme)&&muba_ieq(a_domain,b_domain);}
#endif /* MULIB_WEB_SITE_H */
