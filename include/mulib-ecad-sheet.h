/* MuLIB / Amb3r: ecad-sheet. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_SHEET in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_SHEET_H
#define MULIB_ECAD_SHEET_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t id,parent;const char *name;} ma_sheet;
int mulib_ecad_sheet_order(const ma_sheet *s,size_t n,size_t *order);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_SHEET) && !defined(MULIB_ECAD_SHEET_H_IMPLEMENTED)
#define MULIB_ECAD_SHEET_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_sheet_order(const ma_sheet *s,size_t n,size_t *order){if((!s&&n)||(!order&&n))return 0;ma_edge *e=(ma_edge*)ma_alloc(n,sizeof *e);if(!e)return 0;size_t m=0;for(size_t i=0;i<n;i++){if(!s[i].id){free(e);return 0;}for(size_t j=0;j<i;j++)if(s[j].id==s[i].id){free(e);return 0;}if(s[i].parent){size_t j=0;while(j<n&&s[j].id!=s[i].parent)j++;if(j==n){free(e);return 0;}e[m].from=j;e[m++].to=i;}}int ok=ma_topo(n,e,m,order);free(e);return ok;}
#ifdef __cplusplus
}
#endif
#endif
