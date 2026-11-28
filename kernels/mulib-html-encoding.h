/* MuLIB browser staging addition: html-encoding
 * Implemented subset: BOM detection, explicit transport-label fallback, Windows-1252 default.
 * Remaining: HTML meta prescan and Encoding Standard label resolution.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_ENCODING_H
#define MULIB_HTML_ENCODING_H
#include "detail/mulib-browser-additions-common.h"

static inline const char *mulib_html_encoding_sniff(muba_span s,const char *transport,size_t *skip){if(!skip||!muba_span_ok(s))return NULL;*skip=0;if(s.len>=3&&!memcmp(s.data,"\xef\xbb\xbf",3)){*skip=3;return "utf-8";}if(s.len>=2&&s.data[0]==255&&s.data[1]==254){*skip=2;return "utf-16le";}if(s.len>=2&&s.data[0]==254&&s.data[1]==255){*skip=2;return "utf-16be";}return transport&&*transport?transport:"windows-1252";}
#endif /* MULIB_HTML_ENCODING_H */
