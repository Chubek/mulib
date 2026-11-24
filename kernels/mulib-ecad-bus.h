/* MuLIB: ecad-bus. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_BUS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_BUS_H
#define MULIB_ECAD_BUS_H
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

int mulib_ecad_bus_parse(const char *s,int64_t *first,int64_t *last,size_t *width);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_BUS) && !defined(MULIB_ECAD_BUS_H_IMPLEMENTED)
#define MULIB_ECAD_BUS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_bus_parse(const char *s,int64_t *first,int64_t *last,size_t *width){if(!s||!first||!last||!width)return 0;const char *p=strchr(s,'[');if(!p||p==s)return 0;char *end;errno=0;long long a=strtoll(p+1,&end,10);if(errno||end==p+1||*end!=':')return 0;p=end+1;errno=0;long long b=strtoll(p,&end,10);if(errno||end==p||strcmp(end,"]"))return 0;uint64_t d=a>=b?(uint64_t)a-(uint64_t)b:(uint64_t)b-(uint64_t)a;if(d>=SIZE_MAX)return 0;*first=a;*last=b;*width=(size_t)d+1;return 1;}
#ifdef __cplusplus
}
#endif
#endif
