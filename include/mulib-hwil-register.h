/* MuLIB / Amb3r: hwil-register. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_REGISTER in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_REGISTER_H
#define MULIB_HWIL_REGISTER_H
#include "mulib-amb3r-base.h"
#include "mulib-hwil-context.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_hwil_register_next(const ma_hw_context *c,const uint64_t *values,uint64_t *state);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_REGISTER) && !defined(MULIB_HWIL_REGISTER_H_IMPLEMENTED)
#define MULIB_HWIL_REGISTER_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_register_next(const ma_hw_context *c,const uint64_t *values,uint64_t *state){if(!c||!values||!state)return 0;for(size_t i=0;i<c->n;i++)if(!ma_hw_node_valid(c,i))return 0;for(size_t i=0;i<c->n;i++)if(c->nodes[i].op==MA_HW_REG)state[i]=values[c->nodes[i].arg[0]]&ma_hw_mask(c->nodes[i].width);return 1;}
#ifdef __cplusplus
}
#endif
#endif
