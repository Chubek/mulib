/* MuLIB browser staging addition: html-elements
 * Implemented subset: Classification of HTML void elements.
 * Remaining: Interface registry, namespace and specialized element behavior.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_ELEMENTS_H
#define MULIB_HTML_ELEMENTS_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_html_elements_void(muba_span tag){const char *v[]={"area","base","br","col","embed","hr","img","input","link","meta","param","source","track","wbr"};for(size_t i=0;i<sizeof v/sizeof *v;i++)if(muba_ieq(tag,muba_str(v[i])))return 1;return 0;}
#endif /* MULIB_HTML_ELEMENTS_H */
