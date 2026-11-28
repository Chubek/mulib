/* MuLIB browser staging addition: css-mediaquery
 * Implemented subset: Evaluate individual width/height min/max/exact predicates.
 * Remaining: Media query grammar, boolean composition, features and viewport updates.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_MEDIAQUERY_H
#define MULIB_CSS_MEDIAQUERY_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_mediaquery_compare(double actual,double value,int relation){if(!isfinite(actual)||!isfinite(value)||actual<0||value<0)return 0;return relation<0?actual<=value:relation>0?actual>=value:actual==value;}
#endif /* MULIB_CSS_MEDIAQUERY_H */
