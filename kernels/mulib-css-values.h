/* MuLIB browser staging addition: css-values
 * Implemented subset: Parse a bounded CSS number and optional ASCII unit suffix.
 * Remaining: CSS tokenization, functions, escapes, percentages and full property grammar.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_CSS_VALUES_H
#define MULIB_CSS_VALUES_H
#include "detail/mulib-browser-additions-common.h"
#include "mulib-floatparse.h"
typedef struct {double number;muba_span unit;} mulib_css_values_t;
static inline int mulib_css_values_parse(muba_span s,mulib_css_values_t *out){if(!out||!muba_span_ok(s))return 0;s=muba_trim(s);size_t i=0;if(i<s.len&&(s.data[i]=='+'||s.data[i]=='-'))i++;while(i<s.len&&muba_digit(s.data[i]))i++;if(i<s.len&&s.data[i]=='.'){i++;while(i<s.len&&muba_digit(s.data[i]))i++;}double n;if(!mulib_floatparse_decimal((muba_span){s.data,i},&n)||!isfinite(n))return 0;for(size_t j=i;j<s.len;j++)if(!muba_alpha(s.data[j])&&!(j==i&&s.len-i==1&&s.data[j]=='%'))return 0;*out=(mulib_css_values_t){n,{s.data+i,s.len-i}};return 1;}
#endif /* MULIB_CSS_VALUES_H */
