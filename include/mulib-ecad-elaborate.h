/* MuLIB / Amb3r: ecad-elaborate. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_ELABORATE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_ELABORATE_H
#define MULIB_ECAD_ELABORATE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_ecad_elaborate(size_t nets,const ma_edge *connections,size_t n,size_t *canonical);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_ELABORATE) && !defined(MULIB_ECAD_ELABORATE_H_IMPLEMENTED)
#define MULIB_ECAD_ELABORATE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_elaborate(size_t nets,const ma_edge *connections,size_t n,size_t *canonical){if((!connections&&n)||(!canonical&&nets))return 0;for(size_t i=0;i<n;i++)if(connections[i].from>=nets||connections[i].to>=nets)return 0;for(size_t i=0;i<nets;i++)canonical[i]=i;for(size_t i=0;i<n;i++)ma_union(canonical,connections[i].from,connections[i].to);for(size_t i=0;i<nets;i++)canonical[i]=ma_find(canonical,i);return 1;}
#ifdef __cplusplus
}
#endif
#endif
