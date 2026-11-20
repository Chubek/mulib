/* MuLIB: hwil-lower. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_LOWER in exactly one implementation translation unit.
 * Profile: Single-bit ripple carry adder lowering only.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_LOWER_H
#define MULIB_HWIL_LOWER_H
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
#include "mulib-hwil-builder.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hwil_lower_adder(mulib_hwil_hw_context *c,const size_t *a,const size_t *b,size_t width,size_t carry,size_t *sum,size_t *carry_out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_LOWER) && !defined(MULIB_HWIL_LOWER_H_IMPLEMENTED)
#define MULIB_HWIL_LOWER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_lower_adder(mulib_hwil_hw_context *c,const size_t *a,const size_t *b,size_t width,size_t carry,size_t *sum,size_t *carry_out){if(!c||!a||!b||!sum||!carry_out||!width||carry>=c->n)return 0;size_t original=c->n;for(size_t i=0;i<width;i++){size_t v[2]={a[i],b[i]},x,y,z;if(!mulib_hwil_builder_node(c,MULIB_HWIL_HW_XOR,1,v,2,0,&x)||!mulib_hwil_builder_node(c,MULIB_HWIL_HW_AND,1,v,2,0,&y))goto fail;v[0]=x;v[1]=carry;if(!mulib_hwil_builder_node(c,MULIB_HWIL_HW_XOR,1,v,2,0,sum+i)||!mulib_hwil_builder_node(c,MULIB_HWIL_HW_AND,1,v,2,0,&z))goto fail;v[0]=y;v[1]=z;if(!mulib_hwil_builder_node(c,MULIB_HWIL_HW_OR,1,v,2,0,&carry))goto fail;}*carry_out=carry;return 1;fail:c->n=original;return 0;}
#ifdef __cplusplus
}
#endif
#endif
