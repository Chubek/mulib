/* MuLIB / Amb3r: eda-diagnostics. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_DIAGNOSTICS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_DIAGNOSTICS_H
#define MULIB_EDA_DIAGNOSTICS_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {unsigned severity;uint32_t code;uint64_t object;char *message;} ma_diagnostic;
typedef struct {ma_diagnostic *v;size_t n,cap;} ma_diagnostics;
int mulib_eda_diagnostics_add(ma_diagnostics *d,unsigned severity,uint32_t code,uint64_t object,const char *message);
void mulib_eda_diagnostics_free(ma_diagnostics *d);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_DIAGNOSTICS) && !defined(MULIB_EDA_DIAGNOSTICS_H_IMPLEMENTED)
#define MULIB_EDA_DIAGNOSTICS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_diagnostics_add(ma_diagnostics *d,unsigned severity,uint32_t code,uint64_t object,const char *message){if(!d||severity>3||!code||!message)return 0;ma_diagnostic r={severity,code,object,ma_strdup(message)};if(!r.message||!ma_push((void**)&d->v,&d->n,&d->cap,sizeof r,&r)){free(r.message);return 0;}return 1;}
void mulib_eda_diagnostics_free(ma_diagnostics *d){if(d){for(size_t i=0;i<d->n;i++)free(d->v[i].message);free(d->v);memset(d,0,sizeof *d);}}
#ifdef __cplusplus
}
#endif
#endif
