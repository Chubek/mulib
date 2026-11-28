/* MuLIB browser staging addition: html-attributes
 * Implemented subset: Boolean reflection and bounded enumerated attribute lookup.
 * Remaining: Full reflection, invalid/missing defaults and URL attributes.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_ATTRIBUTES_H
#define MULIB_HTML_ATTRIBUTES_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_html_attributes_boolean(int present){return !!present;}
static inline size_t mulib_html_attributes_enum(muba_span v,const char *const *values,size_t n,size_t fallback){if(values)for(size_t i=0;i<n;i++)if(values[i]&&muba_ieq(v,muba_str(values[i])))return i;return fallback;}
#endif /* MULIB_HTML_ATTRIBUTES_H */
