/* MuLIB browser staging addition: floatformat
 * Implemented subset: Round-trip candidate search using snprintf/strtod in current C locale.
 * Remaining: Correct shortest decimal and ECMAScript formatting; locale independence.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_FLOATFORMAT_H
#define MULIB_FLOATFORMAT_H
#include "detail/mulib-browser-additions-common.h"

static inline int mulib_floatformat_format(double n,char *out,size_t cap){if(!out||!cap)return 0;char b[128];if(isnan(n))strcpy(b,"NaN");else if(isinf(n))strcpy(b,n<0?"-Infinity":"Infinity");else if(n==0)strcpy(b,"0");else{for(int p=1;p<=17;p++){int r=snprintf(b,sizeof b,"%.*g",p,n);if(r<0||(size_t)r>=sizeof b)return 0;char *e;double x=strtod(b,&e);if(!*e&&x==n)break;}}size_t l=strlen(b);if(l>=cap)return 0;memcpy(out,b,l+1);return 1;}
#endif /* MULIB_FLOATFORMAT_H */
