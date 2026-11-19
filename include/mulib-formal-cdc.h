/* MuLIB / Amb3r: formal-cdc. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_CDC in exactly one implementation translation unit.
 * Profile: Check declared domain crossings and synchronization flags, not infer CDC from RTL.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_CDC_H
#define MULIB_FORMAL_CDC_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint32_t source_domain,destination_domain;unsigned synchronized;} ma_clock_crossing;
size_t mulib_formal_cdc(const ma_clock_crossing *edges,size_t n,size_t *violations,size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_CDC) && !defined(MULIB_FORMAL_CDC_H_IMPLEMENTED)
#define MULIB_FORMAL_CDC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_formal_cdc(const ma_clock_crossing *edges,size_t n,size_t *violations,size_t capacity){if(!edges&&n)return MA_NONE;size_t count=0;for(size_t i=0;i<n;i++)if(edges[i].source_domain!=edges[i].destination_domain&&!edges[i].synchronized){if(violations&&count<capacity)violations[count]=i;count++;}return count;}
#ifdef __cplusplus
}
#endif
#endif
