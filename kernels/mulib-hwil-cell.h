/* MuLIB: hwil-cell. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_CELL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_CELL_H
#define MULIB_HWIL_CELL_H
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

int mulib_hwil_cell_eval(const mulib_hwil_hw_node *n,const uint64_t *args,size_t argc,uint64_t *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_CELL) && !defined(MULIB_HWIL_CELL_H_IMPLEMENTED)
#define MULIB_HWIL_CELL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_cell_eval(const mulib_hwil_hw_node *n,const uint64_t *args,size_t argc,uint64_t *out){if(!n||!out||!n->width||n->width>64||n->argc!=argc||(!args&&argc))return 0;uint64_t x;switch(n->op){case MULIB_HWIL_HW_CONST:if(argc)return 0;x=n->constant;break;case MULIB_HWIL_HW_NOT:if(argc!=1)return 0;x=~args[0];break;case MULIB_HWIL_HW_AND:case MULIB_HWIL_HW_OR:case MULIB_HWIL_HW_XOR:case MULIB_HWIL_HW_ADD:if(argc!=2)return 0;x=n->op==MULIB_HWIL_HW_AND?args[0]&args[1]:n->op==MULIB_HWIL_HW_OR?args[0]|args[1]:n->op==MULIB_HWIL_HW_XOR?args[0]^args[1]:args[0]+args[1];break;case MULIB_HWIL_HW_MUX:if(argc!=3)return 0;x=args[0]?args[1]:args[2];break;default:return 0;}*out=x&mulib_hwil_hw_mask(n->width);return 1;}
#ifdef __cplusplus
}
#endif
#endif
