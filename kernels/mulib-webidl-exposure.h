/* MuLIB browser staging addition: webidl-exposure
 * Implemented subset: Realm-mask, secure context and feature gate predicate.
 * Remaining: Extended attributes, per-interface/member exposure and install integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEBIDL_EXPOSURE_H
#define MULIB_WEBIDL_EXPOSURE_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_webidl_exposure_allowed(unsigned actual,unsigned allowed,int secure_required,int secure,int feature){return (actual&allowed)!=0&&(!secure_required||secure)&&feature;}
#endif /* MULIB_WEBIDL_EXPOSURE_H */
