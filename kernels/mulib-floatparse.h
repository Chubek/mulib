/* MuLIB browser staging addition: floatparse
 * Implemented subset: Bounded decimal syntax validation and C strtod conversion.
 * Remaining: Locale-independent correct rounding and ECMAScript grammar.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FLOATPARSE_H
#define MULIB_FLOATPARSE_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_floatparse_decimal(muba_span s,double *out){if(!out||!muba_span_ok(s)||!s.len||s.len>1023)return 0;size_t i=0,d=0;if(s.data[i]=='+'||s.data[i]=='-')i++;while(i<s.len&&muba_digit(s.data[i])){i++;d++;}if(i<s.len&&s.data[i]=='.'){i++;while(i<s.len&&muba_digit(s.data[i])){i++;d++;}}if(!d)return 0;if(i<s.len&&(s.data[i]=='e'||s.data[i]=='E')){i++;if(i<s.len&&(s.data[i]=='+'||s.data[i]=='-'))i++;size_t j=i;while(i<s.len&&muba_digit(s.data[i]))i++;if(i==j)return 0;}if(i!=s.len)return 0;char b[1024],*end;memcpy(b,s.data,s.len);b[s.len]=0;double v=strtod(b,&end);if(end!=b+s.len)return 0;*out=v;return 1;}
#endif /* MULIB_FLOATPARSE_H */
