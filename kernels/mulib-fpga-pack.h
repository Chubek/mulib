/* MuLIB: fpga-pack. C11 single-header kernel.
 * Define MULIB_IMPL_FPGA_PACK in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FPGA_PACK_H
#define MULIB_FPGA_PACK_H
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
#include "mulib-num-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_fpga_pack(const size_t *demand,size_t n,size_t tile_capacity,size_t *tile,size_t *tiles);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FPGA_PACK) && !defined(MULIB_FPGA_PACK_H_IMPLEMENTED)
#define MULIB_FPGA_PACK_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_fpga_pack(const size_t *demand,size_t n,size_t tile_capacity,size_t *tile,size_t *tiles){if((!demand&&n)||(!tile&&n)||!tile_capacity||!tiles)return 0;size_t *order=(size_t*)mulib_storage_alloc(n,sizeof *order),*used=(size_t*)mulib_storage_alloc(n,sizeof *used);if(!order||!used){free(order);free(used);return 0;}for(size_t i=0;i<n;i++){if(!demand[i]||demand[i]>tile_capacity){free(order);free(used);return 0;}size_t j=i;while(j&&demand[order[j-1]]<demand[i]){order[j]=order[j-1];j--;}order[j]=i;}size_t count=0;for(size_t k=0;k<n;k++){size_t j=0,i=order[k];while(j<count&&demand[i]>tile_capacity-used[j])j++;if(j==count)count++;used[j]+=demand[i];tile[i]=j;}*tiles=count;free(order);free(used);return 1;}
#ifdef __cplusplus
}
#endif
#endif
