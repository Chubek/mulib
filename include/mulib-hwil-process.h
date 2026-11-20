/* MuLIB: hwil-process. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_PROCESS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_PROCESS_H
#define MULIB_HWIL_PROCESS_H
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
#include "mulib-hwil-cell.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hwil_process_eval(const mulib_hwil_hw_context *c,const uint64_t *inputs,size_t count,const uint64_t *state,uint64_t *values);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_PROCESS) && !defined(MULIB_HWIL_PROCESS_H_IMPLEMENTED)
#define MULIB_HWIL_PROCESS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_process_eval(const mulib_hwil_hw_context *c,const uint64_t *inputs,size_t count,const uint64_t *state,uint64_t *values){if(!c||!values||(!inputs&&count))return 0;size_t in=0;for(size_t i=0;i<c->n;i++){const mulib_hwil_hw_node *n=c->nodes+i;if(!mulib_hwil_hw_node_valid(c,i))return 0;if(n->op==MULIB_HWIL_HW_INPUT){if(in>=count)return 0;values[i]=inputs[in++]&mulib_hwil_hw_mask(n->width);}else if(n->op==MULIB_HWIL_HW_REG){if(!state)return 0;values[i]=state[i]&mulib_hwil_hw_mask(n->width);}else{uint64_t args[3]={0};for(unsigned j=0;j<n->argc;j++)args[j]=values[n->arg[j]];if(!mulib_hwil_cell_eval(n,args,n->argc,values+i))return 0;}}return in==count;}
#ifdef __cplusplus
}
#endif
#endif
