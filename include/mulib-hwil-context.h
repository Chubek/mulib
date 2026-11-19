/* MuLIB / Amb3r: hwil-context. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_CONTEXT in exactly one implementation translation unit.
 * Profile: Dynamically sized width 1–64 node graph; this representation is independent of the preexisting HWIL module kernel.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_CONTEXT_H
#define MULIB_HWIL_CONTEXT_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {MA_HW_INPUT,MA_HW_CONST,MA_HW_NOT,MA_HW_AND,MA_HW_OR,MA_HW_XOR,MA_HW_ADD,MA_HW_MUX,MA_HW_REG};
typedef struct {unsigned op,width,argc;size_t arg[3];uint64_t constant;} ma_hw_node;
typedef struct {ma_hw_node *nodes;size_t n,cap;} ma_hw_context;
static inline uint64_t ma_hw_mask(unsigned w){return w>=64?UINT64_MAX:w?((UINT64_C(1)<<w)-1):0;}
static inline int ma_hw_node_valid(const ma_hw_context *c,size_t i){const ma_hw_node *n=c->nodes+i;if(!n->width||n->width>64||n->op>MA_HW_REG)return 0;unsigned argc=n->op<=MA_HW_CONST?0:n->op==MA_HW_NOT||n->op==MA_HW_REG?1:n->op==MA_HW_MUX?3:2;if(n->argc!=argc)return 0;for(unsigned j=0;j<argc;j++){if(n->arg[j]>=c->n||(n->op!=MA_HW_REG&&n->arg[j]>=i))return 0;if(c->nodes[n->arg[j]].width!=(n->op==MA_HW_MUX&&j==0?1:n->width))return 0;}return 1;}
int mulib_hwil_context_add(ma_hw_context *c,ma_hw_node n,size_t *id);
void mulib_hwil_context_free(ma_hw_context *c);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_CONTEXT) && !defined(MULIB_HWIL_CONTEXT_H_IMPLEMENTED)
#define MULIB_HWIL_CONTEXT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_context_add(ma_hw_context *c,ma_hw_node n,size_t *id){if(!c||!id||!n.width||n.width>64||n.op>MA_HW_REG||n.argc>3)return 0;for(size_t i=0;i<n.argc;i++)if(n.arg[i]>=c->n)return 0;*id=c->n;return ma_push((void**)&c->nodes,&c->n,&c->cap,sizeof n,&n);}
void mulib_hwil_context_free(ma_hw_context *c){if(c){free(c->nodes);memset(c,0,sizeof *c);}}
#ifdef __cplusplus
}
#endif
#endif
