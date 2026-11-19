/* MuLIB / Amb3r: hwil-flatten. C11 single-header kernel.
 * Define MULIB_IMPL_HWIL_FLATTEN in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HWIL_FLATTEN_H
#define MULIB_HWIL_FLATTEN_H
#include "mulib-amb3r-base.h"
#include "mulib-hwil-elaborate.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {const ma_hw_context *module;const size_t *inputs;size_t ninputs;size_t *mapping;} ma_hw_instance_request;
int mulib_hwil_flatten_instances(ma_hw_context *dest,const ma_hw_instance_request *instances,size_t n);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HWIL_FLATTEN) && !defined(MULIB_HWIL_FLATTEN_H_IMPLEMENTED)
#define MULIB_HWIL_FLATTEN_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hwil_flatten_instances(ma_hw_context *dest,const ma_hw_instance_request *instances,size_t n){if(!dest||(!instances&&n))return 0;size_t base=dest->n;for(size_t i=0;i<n;i++){const ma_hw_context *c=instances[i].module;if(!c||!mulib_hwil_elaborate(c,dest,instances[i].inputs,instances[i].ninputs,instances[i].mapping)){dest->n=base;return 0;}}return 1;}
#ifdef __cplusplus
}
#endif
#endif
