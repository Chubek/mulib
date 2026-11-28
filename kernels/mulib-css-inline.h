/* MuLIB browser staging addition: css-inline
 * Implemented subset: Greedy line wrapping of caller-measured items.
 * Remaining: Inline formatting, bidi, baseline, whitespace and Unicode line breaking.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_INLINE_H
#define MULIB_CSS_INLINE_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_inline_wrap(const double *width,size_t n,double limit,size_t *line){if((!width||!line)&&n)return 0;if(!isfinite(limit)||limit<0)return 0;for(size_t i=0;i<n;i++)if(!isfinite(width[i])||width[i]<0)return 0;double used=0;size_t l=0;for(size_t i=0;i<n;i++){if(used>0&&width[i]>limit-used){l++;used=0;}line[i]=l;used+=width[i];}return 1;}
#endif /* MULIB_CSS_INLINE_H */
