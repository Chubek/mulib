/* MuLIB browser staging addition: css-pseudoelement
 * Implemented subset: Recognize common pseudo-element identifiers.
 * Remaining: Generated content, style inheritance, tree and layout integration.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_PSEUDOELEMENT_H
#define MULIB_CSS_PSEUDOELEMENT_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_css_pseudoelement_kind(muba_span name){const char *v[]={"before","after","first-line","first-letter","marker","placeholder","selection"};for(size_t i=0;i<sizeof v/sizeof *v;i++)if(muba_ieq(name,muba_str(v[i])))return (int)i+1;return 0;}
#endif /* MULIB_CSS_PSEUDOELEMENT_H */
