/* MuLIB browser staging addition: css-fontface
 * Implemented subset: Font-face weight-range and family matching.
 * Remaining: Font loading, source descriptors, fallback and shaping integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_FONTFACE_H
#define MULIB_CSS_FONTFACE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {muba_span family;unsigned weight_min,weight_max;uintptr_t font;} mulib_css_fontface_face;
static inline uintptr_t mulib_css_fontface_match(const mulib_css_fontface_face *v,size_t n,muba_span family,unsigned weight){if(v)for(size_t i=0;i<n;i++)if(muba_ieq(v[i].family,family)&&weight>=v[i].weight_min&&weight<=v[i].weight_max)return v[i].font;return 0;}
#endif /* MULIB_CSS_FONTFACE_H */
