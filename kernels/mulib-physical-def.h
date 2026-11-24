/* MuLIB: physical-def. C11 single-header kernel.
 * Define MULIB_IMPL_PHYSICAL_DEF in exactly one implementation translation unit.
 * Profile: PLACED components fragment writer only.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PHYSICAL_DEF_H
#define MULIB_PHYSICAL_DEF_H
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
typedef struct {const char *name,*macro;int64_t x,y;unsigned orientation;} mulib_physical_def_component;
int mulib_physical_def_components(FILE *f,const mulib_physical_def_component *items,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PHYSICAL_DEF) && !defined(MULIB_PHYSICAL_DEF_H_IMPLEMENTED)
#define MULIB_PHYSICAL_DEF_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_physical_def_components(FILE *f,const mulib_physical_def_component *items,size_t n){if(!f||(!items&&n))return 0;static const char *const rot[]={"N","E","S","W","FN","FE","FS","FW"};for(size_t i=0;i<n;i++)if(!mulib_storage_identifier_valid(items[i].name)||!mulib_storage_identifier_valid(items[i].macro)||items[i].orientation>=8)return 0;if(fprintf(f,"COMPONENTS %zu ;\n",n)<0)return 0;for(size_t i=0;i<n;i++)if(fprintf(f,"- %s %s + PLACED ( %lld %lld ) %s ;\n",items[i].name,items[i].macro,(long long)items[i].x,(long long)items[i].y,rot[items[i].orientation])<0)return 0;return fputs("END COMPONENTS\n",f)>=0;}
#ifdef __cplusplus
}
#endif
#endif
