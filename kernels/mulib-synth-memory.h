/* MuLIB: synth-memory. C11 single-header kernel.
 * Define MULIB_IMPL_SYNTH_MEMORY in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_SYNTH_MEMORY_H
#define MULIB_SYNTH_MEMORY_H
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
#ifdef __cplusplus
extern "C" {
#endif

int mulib_synth_memory_decode(unsigned address_bits,size_t depth,uint64_t address,unsigned char *select);
size_t mulib_synth_memory_address_bits(size_t depth);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_SYNTH_MEMORY) && !defined(MULIB_SYNTH_MEMORY_H_IMPLEMENTED)
#define MULIB_SYNTH_MEMORY_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_synth_memory_decode(unsigned address_bits,size_t depth,uint64_t address,unsigned char *select){if(!address_bits||address_bits>64||!depth||!select||(address_bits<64&&depth>((uint64_t)1<<address_bits))||address>=depth)return 0;memset(select,0,depth);select[address]=1;return 1;}
size_t mulib_synth_memory_address_bits(size_t depth){if(!depth)return 0;size_t bits=0,x=depth-1;while(x){bits++;x>>=1;}return bits?bits:1;}
#ifdef __cplusplus
}
#endif
#endif
