/* MuLIB browser staging addition: locale-segment
 * Implemented subset: UTF-8 code point boundary iteration.
 * Remaining: Grapheme, word and sentence segmentation using Unicode/CLDR.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_LOCALE_SEGMENT_H
#define MULIB_LOCALE_SEGMENT_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_locale_segment_next(muba_span s,size_t *pos,muba_span *segment){if(!pos||!segment)return 0;size_t p=*pos;uint32_t c;if(!muba_utf8_next(s,&p,&c,0))return 0;*segment=(muba_span){s.data+*pos,p-*pos};*pos=p;return 1;}
#endif /* MULIB_LOCALE_SEGMENT_H */
