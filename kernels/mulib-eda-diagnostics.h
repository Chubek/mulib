/* MuLIB: eda-diagnostics. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_DIAGNOSTICS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_DIAGNOSTICS_H
#define MULIB_EDA_DIAGNOSTICS_H
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
typedef struct {unsigned severity;uint32_t code;uint64_t object;char *message;} mulib_eda_diagnostic_record;
typedef struct {mulib_eda_diagnostic_record *v;size_t n,cap;} mulib_eda_diagnostics;
int mulib_eda_diagnostics_add(mulib_eda_diagnostics *d,unsigned severity,uint32_t code,uint64_t object,const char *message);
void mulib_eda_diagnostics_free(mulib_eda_diagnostics *d);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_DIAGNOSTICS) && !defined(MULIB_EDA_DIAGNOSTICS_H_IMPLEMENTED)
#define MULIB_EDA_DIAGNOSTICS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_diagnostics_add(mulib_eda_diagnostics *d,unsigned severity,uint32_t code,uint64_t object,const char *message){if(!d||severity>3||!code||!message)return 0;mulib_eda_diagnostic_record r={severity,code,object,mulib_storage_string_dup(message)};if(!r.message||!mulib_storage_push((void**)&d->v,&d->n,&d->cap,sizeof r,&r)){free(r.message);return 0;}return 1;}
void mulib_eda_diagnostics_free(mulib_eda_diagnostics *d){if(d){for(size_t i=0;i<d->n;i++)free(d->v[i].message);free(d->v);memset(d,0,sizeof *d);}}
#ifdef __cplusplus
}
#endif
#endif
