/* MuLIB browser staging addition: html-custom-element
 * Implemented subset: ASCII custom-element name validation including reserved names.
 * Remaining: Full name grammar, registry, reactions, construction stack and upgrade.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_CUSTOM_ELEMENT_H
#define MULIB_HTML_CUSTOM_ELEMENT_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_html_custom_element_valid(muba_span s){if(!muba_span_ok(s)||s.len<2||s.data[0]<'a'||s.data[0]>'z')return 0;int dash=0;for(size_t i=0;i<s.len;i++){unsigned c=s.data[i];if(c=='-')dash=1;else if(!((c>='a'&&c<='z')||muba_digit((unsigned char)c)||c=='.'||c=='_'))return 0;}const char *bad[]={"annotation-xml","color-profile","font-face","font-face-src","font-face-uri","font-face-format","font-face-name","missing-glyph"};for(size_t i=0;i<8;i++)if(muba_eq(s,muba_str(bad[i])))return 0;return dash;}
#endif /* MULIB_HTML_CUSTOM_ELEMENT_H */
