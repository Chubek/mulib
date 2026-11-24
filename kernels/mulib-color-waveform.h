/** @file mulib-color-waveform.h
 * Luma or RGB waveform density bins.
 * C11 header-only; caller owns buffers unless expressly stated.
 */
#ifndef MULIB_COLOR_WAVEFORM_H
#define MULIB_COLOR_WAVEFORM_H
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
/* bin_cap is uint32_t elements; bins cannot overlap s. channel: 0=luma,
 * 1=R, 2=G, 3=B. Skip nonfinite selected values; clamp finite HDR to edges. */
static inline bool mulib_color_waveform(mulib_img_view s,uint32_t *bins,size_t bin_cap,size_t cols,size_t levels,int channel){
    size_t n,bytes;
    if(!mulib_img_valid(s)||!bins||!cols||cols>8192||levels<2||levels>4096||channel<0||channel>3||
       !mulib_media_size_mul(cols,levels,&n)||n>bin_cap||
       !mulib_img_memory_span(bins,n,sizeof *bins,&bytes)||
       !mulib_img_disjoint_array(s,bins,n,sizeof *bins))return false;
    memset(bins,0,bytes);
    for(size_t y=0;y<s.height;y++)for(size_t x=0;x<s.width;x++){
        mulib_img_rgba c=mulib_img_get(s,x,y);
        double v=channel==0?.2126*(double)c.r+.7152*(double)c.g+.0722*(double)c.b:
                 channel==1?c.r:channel==2?c.g:c.b;
        if(!isfinite(v))continue;
        size_t i=(size_t)((uint64_t)x*cols/s.width);
        size_t j=(size_t)(fmax(0,fmin(v,1))*(levels-1));
        if(bins[j*cols+i]!=UINT32_MAX)bins[j*cols+i]++;
    }
    return true;
}
#ifdef __cplusplus
}
#endif
#endif
