/* MuLIB browser staging addition: webidl-overload
 * Implemented subset: Arity-based unique candidate selection.
 * Remaining: Distinguishing argument types, variadics, overload conversion and errors.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEBIDL_OVERLOAD_H
#define MULIB_WEBIDL_OVERLOAD_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {size_t min,max;uint32_t id;} mulib_webidl_overload_candidate;
static inline int mulib_webidl_overload_select(const mulib_webidl_overload_candidate *v,size_t n,size_t argc,uint32_t *id){if(!id||(!v&&n))return 0;size_t matches=0;uint32_t r=0;for(size_t i=0;i<n;i++)if(argc>=v[i].min&&argc<=v[i].max){r=v[i].id;matches++;}if(matches!=1)return 0;*id=r;return 1;}
#endif /* MULIB_WEBIDL_OVERLOAD_H */
