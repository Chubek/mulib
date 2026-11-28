/* MuLIB browser staging addition: timezone
 * Implemented subset: Fixed offset conversion and validated transition lookup.
 * Remaining: IANA tzdb, DST ambiguity and historical calendar rules.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_TIMEZONE_H
#define MULIB_TIMEZONE_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {int64_t at;int32_t offset;} mulib_timezone_transition;
static inline int mulib_timezone_offset(const mulib_timezone_transition *v,size_t n,int64_t t,int32_t initial,int32_t *out){if(!out||(!v&&n))return 0;for(size_t i=1;i<n;i++)if(v[i].at<=v[i-1].at)return 0;size_t a=0,b=n;while(a<b){size_t m=a+(b-a)/2;if(v[m].at<=t)a=m+1;else b=m;}*out=a?v[a-1].offset:initial;return 1;}
static inline int mulib_timezone_local(int64_t t,int32_t off,int64_t *out){if(!out||(off>0&&t>INT64_MAX-off)||(off<0&&t<INT64_MIN-off))return 0;*out=t+off;return 1;}
#endif /* MULIB_TIMEZONE_H */
