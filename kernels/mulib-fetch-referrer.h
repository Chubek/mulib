/* MuLIB browser staging addition: fetch-referrer
 * Implemented subset: Decision for no-referrer, origin, strict-origin and strict-origin-when-cross-origin.
 * Remaining: Full policy parser, URL stripping and document integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FETCH_REFERRER_H
#define MULIB_FETCH_REFERRER_H
#include "detail/mulib-browser-additions-common.h"

typedef enum {mulib_fetch_referrer_NONE,mulib_fetch_referrer_ORIGIN,mulib_fetch_referrer_STRICT_ORIGIN,mulib_fetch_referrer_STRICT_CROSS} mulib_fetch_referrer_policy;
/* 0 omit, 1 origin only, 2 sanitized full URL. */
static inline int mulib_fetch_referrer_choose(mulib_fetch_referrer_policy p,int same,int downgrade){switch(p){case mulib_fetch_referrer_NONE:return 0;case mulib_fetch_referrer_ORIGIN:return 1;case mulib_fetch_referrer_STRICT_ORIGIN:return downgrade?0:1;case mulib_fetch_referrer_STRICT_CROSS:return same?2:downgrade?0:1;default:return 0;}}
#endif /* MULIB_FETCH_REFERRER_H */
