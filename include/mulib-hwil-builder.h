/* MuLIB: hwil-builder. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_BUILDER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_BUILDER_H
#define MULIB_HWIL_BUILDER_H
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

int mulib_hwil_builder_node(mulib_hwil_hw_context *c,unsigned op,unsigned width,const size_t *args,unsigned argc,uint64_t constant,size_t *id);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_BUILDER) && !defined(MULIB_HWIL_BUILDER_H_IMPLEMENTED)
#define MULIB_HWIL_BUILDER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_builder_node(mulib_hwil_hw_context *c,unsigned op,unsigned width,const size_t *args,unsigned argc,uint64_t constant,size_t *id){if(!c||argc>3||(!args&&argc))return 0;mulib_hwil_hw_node n={op,width,argc,{0,0,0},constant};for(unsigned i=0;i<argc;i++)n.arg[i]=args[i];size_t out;if(!mulib_hwil_context_add(c,n,&out))return 0;if(!mulib_hwil_hw_node_valid(c,out)){c->n--;return 0;}if(id)*id=out;return 1;}
#ifdef __cplusplus
}
#endif
#endif
