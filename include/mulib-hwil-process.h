/* MuLIB / Amb3r: hwil-process. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_PROCESS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_PROCESS_H
#define MULIB_HWIL_PROCESS_H
#include "mulib-amb3r-base.h"
#include "mulib-hwil-cell.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hwil_process_eval(const ma_hw_context *c,const uint64_t *inputs,size_t count,const uint64_t *state,uint64_t *values);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_PROCESS) && !defined(MULIB_HWIL_PROCESS_H_IMPLEMENTED)
#define MULIB_HWIL_PROCESS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_process_eval(const ma_hw_context *c,const uint64_t *inputs,size_t count,const uint64_t *state,uint64_t *values){if(!c||!values||(!inputs&&count))return 0;size_t in=0;for(size_t i=0;i<c->n;i++){const ma_hw_node *n=c->nodes+i;if(!ma_hw_node_valid(c,i))return 0;if(n->op==MA_HW_INPUT){if(in>=count)return 0;values[i]=inputs[in++]&ma_hw_mask(n->width);}else if(n->op==MA_HW_REG){if(!state)return 0;values[i]=state[i]&ma_hw_mask(n->width);}else{uint64_t args[3]={0};for(unsigned j=0;j<n->argc;j++)args[j]=values[n->arg[j]];if(!mulib_hwil_cell_eval(n,args,n->argc,values+i))return 0;}}return in==count;}
#ifdef __cplusplus
}
#endif
#endif
