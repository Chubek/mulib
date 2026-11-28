/* MuLIB browser staging addition: idna
 * Implemented subset: Strict ASCII DNS label validation and lowercase normalization.
 * Remaining: UTS #46 mapping, Punycode integration, Bidi and contextual rules.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_IDNA_H
#define MULIB_IDNA_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_idna_ascii_label(muba_span s,muba_buf *out){if(!out||!muba_span_ok(s)||!s.len||s.len>63||s.data[0]=='-'||s.data[s.len-1]=='-')return 0;muba_buf b={0};for(size_t i=0;i<s.len;i++){unsigned char c=muba_lower(s.data[i]);if(!muba_alpha(c)&&!muba_digit(c)&&c!='-'){muba_free(&b);return 0;}if(!muba_byte(&b,c)){muba_free(&b);return 0;}}return muba_commit(out,&b);}
#endif /* MULIB_IDNA_H */
