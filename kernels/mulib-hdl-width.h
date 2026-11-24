/* MuLIB: hdl-width. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_WIDTH in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_WIDTH_H
#define MULIB_HDL_WIDTH_H
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
#include "mulib-geom2-core.h"
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hdl_width_infer(unsigned op,size_t a,size_t b,size_t context,size_t *width);
uint64_t mulib_hdl_width_extend(uint64_t x,unsigned from,unsigned to,int sign);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_WIDTH) && !defined(MULIB_HDL_WIDTH_H_IMPLEMENTED)
#define MULIB_HDL_WIDTH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_width_infer(unsigned op,size_t a,size_t b,size_t context,size_t *width){if(!width||!a||!b)return 0;switch(op){case 0:*width=MULIB_STORAGE_MAX(context,MULIB_STORAGE_MAX(a,b));break;case 1:*width=1;break;case 2:if(a>SIZE_MAX-b)return 0;*width=a+b;break;case 3:*width=MULIB_STORAGE_MAX(context,a);break;default:return 0;}return 1;}
uint64_t mulib_hdl_width_extend(uint64_t x,unsigned from,unsigned to,int sign){if(!from||from>64||to>64||!to)return 0;x&=mulib_hwil_hw_mask(from);if(sign&&(x&(UINT64_C(1)<<(from-1))))x|=~mulib_hwil_hw_mask(from);return x&mulib_hwil_hw_mask(to);}
#ifdef __cplusplus
}
#endif
#endif
