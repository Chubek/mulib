/* MuLIB: hwil-elaborate. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_ELABORATE in exactly one implementation translation unit.
 * Profile: Flatten single already-elaborated module through input substitution; no general HDL elaboration.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_ELABORATE_H
#define MULIB_HWIL_ELABORATE_H
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

int mulib_hwil_elaborate(const mulib_hwil_hw_context *source,mulib_hwil_hw_context *dest,const size_t *inputs,size_t ninputs,size_t *mapping);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_ELABORATE) && !defined(MULIB_HWIL_ELABORATE_H_IMPLEMENTED)
#define MULIB_HWIL_ELABORATE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_elaborate(const mulib_hwil_hw_context *source,mulib_hwil_hw_context *dest,const size_t *inputs,size_t ninputs,size_t *mapping){if(!source||!dest||source==dest||(!inputs&&ninputs)||(!mapping&&source->n))return 0;size_t n0=dest->n,in=0;for(size_t i=0;i<source->n;i++){mulib_hwil_hw_node node=source->nodes[i];if(!mulib_hwil_hw_node_valid(source,i))goto fail;if(node.op==MULIB_HWIL_HW_INPUT){if(in>=ninputs||inputs[in]>=n0||dest->nodes[inputs[in]].width!=node.width)goto fail;mapping[i]=inputs[in++];continue;}for(unsigned j=0;j<node.argc;j++){if(node.arg[j]>=i)goto fail;node.arg[j]=mapping[node.arg[j]];}if(!mulib_hwil_context_add(dest,node,mapping+i))goto fail;}if(in==ninputs)return 1;fail:dest->n=n0;return 0;}
#ifdef __cplusplus
}
#endif
#endif
