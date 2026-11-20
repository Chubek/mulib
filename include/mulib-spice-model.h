/* MuLIB: spice-model. C11 single-header kernel.
 * Define MULIB_IMPL_SPICE_MODEL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SPICE_MODEL_H
#define MULIB_SPICE_MODEL_H
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
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {const char *name;double value;} mulib_spice_parameter;
int mulib_spice_model_parameter(const mulib_spice_parameter *p,size_t n,const char *key,double fallback,double *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SPICE_MODEL) && !defined(MULIB_SPICE_MODEL_H_IMPLEMENTED)
#define MULIB_SPICE_MODEL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_spice_model_parameter(const mulib_spice_parameter *p,size_t n,const char *key,double fallback,double *out){if((!p&&n)||!key||!out||!isfinite(fallback))return 0;*out=fallback;for(size_t i=0;i<n;i++)if(p[i].name&&!strcmp(p[i].name,key)){if(!isfinite(p[i].value))return 0;*out=p[i].value;return 1;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
