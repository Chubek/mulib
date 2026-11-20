/* MuLIB: hwil-attribute. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_ATTRIBUTE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_ATTRIBUTE_H
#define MULIB_HWIL_ATTRIBUTE_H
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include "mulib-storage-core.h"
#include "mulib-eda-properties.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hwil_attribute_set(mulib_eda_properties *attrs,size_t node,const char *key,mulib_eda_property_value value);
const mulib_eda_property *mulib_hwil_attribute_get(const mulib_eda_properties *attrs,size_t node,const char *key);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_ATTRIBUTE) && !defined(MULIB_HWIL_ATTRIBUTE_H_IMPLEMENTED)
#define MULIB_HWIL_ATTRIBUTE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_attribute_set(mulib_eda_properties *attrs,size_t node,const char *key,mulib_eda_property_value value){if(node==SIZE_MAX)return 0;return mulib_eda_properties_set(attrs,(uint64_t)node+1,key,value);}
const mulib_eda_property *mulib_hwil_attribute_get(const mulib_eda_properties *attrs,size_t node,const char *key){if(node==SIZE_MAX)return NULL;return mulib_eda_properties_get(attrs,(uint64_t)node+1,key);}
#ifdef __cplusplus
}
#endif
#endif
