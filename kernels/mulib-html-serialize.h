/* MuLIB browser staging addition: html-serialize
 * Implemented subset: Escape text and quoted attribute values into owned output.
 * Remaining: Full HTML serialization, namespace, raw text and void handling.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_HTML_SERIALIZE_H
#define MULIB_HTML_SERIALIZE_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_html_serialize_escape(muba_span s,int attribute,muba_buf *out){if(!out||!muba_span_ok(s))return 0;muba_buf b={0};for(size_t i=0;i<s.len;i++){unsigned char c=s.data[i];const char *e=c=='&'?"&amp;":c=='<'?"&lt;":c=='>'?"&gt;":c=='"'&&attribute?"&quot;":NULL;if(!(e?muba_append(&b,muba_str(e)):muba_byte(&b,c))){muba_free(&b);return 0;}}return muba_commit(out,&b);}
#endif /* MULIB_HTML_SERIALIZE_H */
