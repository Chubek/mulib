/* MuLIB: hdl-consteval. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_CONSTEVAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_CONSTEVAL_H
#define MULIB_HDL_CONSTEVAL_H
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
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hdl_consteval(unsigned op,unsigned width,uint64_t a,uint64_t b,uint64_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_CONSTEVAL) && !defined(MULIB_HDL_CONSTEVAL_H_IMPLEMENTED)
#define MULIB_HDL_CONSTEVAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_consteval(unsigned op,unsigned width,uint64_t a,uint64_t b,uint64_t *out){if(!width||width>64||!out)return 0;uint64_t mask=mulib_hwil_hw_mask(width);a&=mask;b&=mask;uint64_t v;switch(op){case 0:v=a+b;break;case 1:v=a-b;break;case 2:v=a*b;break;case 3:if(!b)return 0;v=a/b;break;case 4:if(!b)return 0;v=a%b;break;case 5:v=a&b;break;case 6:v=a|b;break;case 7:v=a^b;break;case 8:v=b>=width?0:a<<b;break;case 9:v=b>=width?0:a>>b;break;default:return 0;}*out=v&mask;return 1;}
#ifdef __cplusplus
}
#endif
#endif
