/* MuLIB: pcb-dft. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_DFT in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_DFT_H
#define MULIB_PCB_DFT_H
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
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#include "mulib-pcb-pad.h"
#ifdef __cplusplus
extern "C" {
#endif

size_t mulib_pcb_dft_uncovered(const uint64_t *nets,size_t n,const mulib_pcb_pad *test,size_t nt,double min_diameter,uint64_t *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_DFT) && !defined(MULIB_PCB_DFT_H_IMPLEMENTED)
#define MULIB_PCB_DFT_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_pcb_dft_uncovered(const uint64_t *nets,size_t n,const mulib_pcb_pad *test,size_t nt,double min_diameter,uint64_t *out,size_t cap){if((!nets&&n)||(!test&&nt)||min_diameter<0)return MULIB_STORAGE_NONE;size_t count=0;for(size_t i=0;i<n;i++){int found=0;for(size_t j=0;j<nt;j++)if(test[j].net==nets[i]&&MULIB_STORAGE_MIN(test[j].width,test[j].height)>=min_diameter)found=1;if(!found){if(out&&count<cap)out[count]=nets[i];count++;}}return count;}
#ifdef __cplusplus
}
#endif
#endif
