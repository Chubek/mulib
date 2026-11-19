/* MuLIB / Amb3r: hwil-cell. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_CELL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_CELL_H
#define MULIB_HWIL_CELL_H
#include "mulib-amb3r-base.h"
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hwil_cell_eval(const ma_hw_node *n,const uint64_t *args,size_t argc,uint64_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_CELL) && !defined(MULIB_HWIL_CELL_H_IMPLEMENTED)
#define MULIB_HWIL_CELL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_cell_eval(const ma_hw_node *n,const uint64_t *args,size_t argc,uint64_t *out){if(!n||!out||!n->width||n->width>64||n->argc!=argc||(!args&&argc))return 0;uint64_t x;switch(n->op){case MA_HW_CONST:if(argc)return 0;x=n->constant;break;case MA_HW_NOT:if(argc!=1)return 0;x=~args[0];break;case MA_HW_AND:case MA_HW_OR:case MA_HW_XOR:case MA_HW_ADD:if(argc!=2)return 0;x=n->op==MA_HW_AND?args[0]&args[1]:n->op==MA_HW_OR?args[0]|args[1]:n->op==MA_HW_XOR?args[0]^args[1]:args[0]+args[1];break;case MA_HW_MUX:if(argc!=3)return 0;x=args[0]?args[1]:args[2];break;default:return 0;}*out=x&ma_hw_mask(n->width);return 1;}
#ifdef __cplusplus
}
#endif
#endif
