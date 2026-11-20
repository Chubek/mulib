/** @file mulib-color-vectorscope.h
 * BT.709 Cb/Cr density histogram to caller-owned square bins.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_COLOR_VECTORSCOPE_H
#define MULIB_COLOR_VECTORSCOPE_H
#include "mulib-img-core.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
/* bin_cap is uint32_t elements. Disjoint bins/source, saturated counts.
 * Nonfinite RGB pixels are skipped; finite HDR coordinates clamp to the edge. */
static inline bool mulib_color_vectorscope(mulib_img_view s,uint32_t *bins,size_t bin_cap,size_t edge){
    size_t n,bytes;
    if(!mulib_img_valid(s)||!bins||edge<2||edge>4096||
       !mulib_media_size_mul(edge,edge,&n)||n>bin_cap||
       !mulib_img_memory_span(bins,n,sizeof *bins,&bytes)||
       !mulib_img_disjoint_array(s,bins,n,sizeof *bins))return false;
    memset(bins,0,bytes);
    for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){
        mulib_img_rgba c=mulib_img_get(s,x,y);
        if(!isfinite(c.r)||!isfinite(c.g)||!isfinite(c.b))continue;
        double l=.2126*(double)c.r+.7152*(double)c.g+.0722*(double)c.b;
        double u=(c.b-l)/1.8556+.5,v=(c.r-l)/1.5748+.5;
        size_t i=(size_t)(fmax(0,fmin(u,1))*(edge-1));
        size_t j=(size_t)(fmax(0,fmin(v,1))*(edge-1));
        if(bins[j*edge+i]!=UINT32_MAX)bins[j*edge+i]++;
    }
    return true;
}
#ifdef __cplusplus
}
#endif
#endif
