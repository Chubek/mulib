/* MuLIB browser staging addition: webidl-dictionary
 * Implemented subset: Required field presence validation and default substitution.
 * Remaining: Dictionary inheritance, JS getters, conversion order and exceptions.
 * C11 static-inline profile; no external implementation switch.
 * SPDX-License-Identifier: MIT
 */
#ifndef MULIB_WEBIDL_DICTIONARY_H
#define MULIB_WEBIDL_DICTIONARY_H
#include "detail/mulib-browser-additions-common.h"

typedef struct {int present,required;uintptr_t value,default_value;} mulib_webidl_dictionary_field;
static inline int mulib_webidl_dictionary_resolve(const mulib_webidl_dictionary_field *f,size_t n,uintptr_t *out){if((!f||!out)&&n)return 0;for(size_t i=0;i<n;i++)if(!f[i].present&&f[i].required)return 0;for(size_t i=0;i<n;i++)out[i]=f[i].present?f[i].value:f[i].default_value;return 1;}
#endif /* MULIB_WEBIDL_DICTIONARY_H */
