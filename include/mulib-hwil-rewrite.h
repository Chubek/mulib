/* MuLIB / Amb3r: hwil-rewrite. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_REWRITE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_REWRITE_H
#define MULIB_HWIL_REWRITE_H
#include "mulib-amb3r-base.h"
#include "mulib-hwil-cell.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_hwil_rewrite_constants(ma_hw_context *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_REWRITE) && !defined(MULIB_HWIL_REWRITE_H_IMPLEMENTED)
#define MULIB_HWIL_REWRITE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_hwil_rewrite_constants(ma_hw_context *c){if(!c)return MA_NONE;for(size_t i=0;i<c->n;i++)if(!ma_hw_node_valid(c,i))return MA_NONE;size_t count=0;for(size_t i=0;i<c->n;i++){ma_hw_node *node=c->nodes+i;if(node->op<=MA_HW_CONST||node->op==MA_HW_REG)continue;uint64_t args[3]={0},result;unsigned j=0;for(;j<node->argc;j++){ma_hw_node *p=c->nodes+node->arg[j];if(p->op!=MA_HW_CONST)break;args[j]=p->constant;}if(j==node->argc&&mulib_hwil_cell_eval(node,args,j,&result)){node->op=MA_HW_CONST;node->argc=0;node->constant=result;memset(node->arg,0,sizeof node->arg);count++;}}return count;}
#ifdef __cplusplus
}
#endif
#endif
