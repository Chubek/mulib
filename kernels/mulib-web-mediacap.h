/* MuLIB browser staging addition: web-mediacap
 * Implemented subset: Explicit host media capability query.
 * Remaining: Codec configuration parsing, hardware and smoothness/power metrics.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEB_MEDIACAP_H
#define MULIB_WEB_MEDIACAP_H
#include "detail/mulib-browser-additions-common.h"

typedef int (*mulib_web_mediacap_query)(void *,muba_span,unsigned,unsigned);
static inline int mulib_web_mediacap_supports(mulib_web_mediacap_query q,void *ctx,muba_span codec,unsigned width,unsigned height){return q&&codec.len&&muba_span_ok(codec)&&width&&height&&q(ctx,codec,width,height)>0;}
#endif /* MULIB_WEB_MEDIACAP_H */
