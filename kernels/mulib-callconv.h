/* MuLIB compiler tools: Deterministic integer/pointer argument placement for AMD64 SysV and RV64 LP64.
 * ISO C11 single-header module. Define MULIB_IMPL_CALLCONV in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_CALLCONV_H
#define MULIB_CALLCONV_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum {MULIB_CALLCONV_AMD64_SYSV,MULIB_CALLCONV_RV64_LP64} mulib_callconv_abi_t;
typedef struct {int reg;
size_t stack_offset;
} mulib_callconv_location_t;
/* Result reg is physical architectural index, -1 stack; 8-byte values only. */
int mulib_callconv_arguments(mulib_callconv_abi_t abi,size_t count,mulib_callconv_location_t *out,size_t *stack_size);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CALLCONV_H */
#if defined(MULIB_IMPL_CALLCONV) && !defined(MULIB_CALLCONV_IMPLEMENTED)
#define MULIB_CALLCONV_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_callconv_arguments(mulib_callconv_abi_t abi,size_t n,mulib_callconv_location_t *out,size_t *stack_size){
 static const int amd64[]={7,6,2,1,8,9};
static const int rv[]={10,11,12,13,14,15,16,17};
 if(!stack_size||(!out&&n)||n>SIZE_MAX/8||n>1048576)return 0;
size_t slots=abi==MULIB_CALLCONV_AMD64_SYSV?6:abi==MULIB_CALLCONV_RV64_LP64?8:0;
if(!slots)return 0;
 for(size_t i=0;i<n;i++){out[i].reg=i<slots?(abi==MULIB_CALLCONV_AMD64_SYSV?amd64[i]:rv[i]):-1;
out[i].stack_offset=i<slots?SIZE_MAX:(i-slots)*8;
}
 *stack_size=(n>slots?n-slots:0)*8;
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
