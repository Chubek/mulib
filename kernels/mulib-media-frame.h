/** @file mulib-media-frame.h
 * Checked strided frame views, heap allocation and row copying.
 * C11 header-only; caller owns buffers unless expressly stated.
 * Scope: Copy rejects intersecting plane storage except an identical layout.
 * Owned and borrowed frames share layout; caller tracks ownership.
 */
#ifndef MULIB_MEDIA_FRAME_H
#define MULIB_MEDIA_FRAME_H
#include "mulib-media-core.h"
#include "mulib-media-pixelformat.h"
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
typedef struct { uint8_t *data[3]; size_t stride[3],capacity[3],width,height;mulib_media_pixelformat format;int64_t pts;mulib_media_rational timebase; } mulib_media_frame;
/* Bounding byte extent including row padding, not the entire capacity.
 * A frame view borrows capacity[p] live bytes at data[p]; counts are bytes. */
static inline bool mulib_media_frame_plane_extent(const mulib_media_frame *f,unsigned p,size_t *out){
 size_t row,n,off;
 if(!f||!out||!mulib_media_pixelformat_layout(f->format,f->width,f->height,p,&row,&n)||
    !f->data[p]||f->stride[p]<row||!mulib_media_size_mul(n-1,f->stride[p],&off)||
    !mulib_media_size_add(off,row,&off)||off>f->capacity[p]||off>UINTPTR_MAX-(uintptr_t)f->data[p])return false;
 *out=off;return true;
}
static inline bool mulib_media_frame_valid(const mulib_media_frame *f){
 if(!f||!mulib_media_pixelformat_planes(f->format))return false;
 size_t ext[3]={0};
 for(unsigned p=0;p<mulib_media_pixelformat_planes(f->format);p++){
  if(!mulib_media_frame_plane_extent(f,p,&ext[p]))return false;
  for(unsigned q=0;q<p;q++)if(!mulib_media_ranges_disjoint(f->data[p],ext[p],f->data[q],ext[q]))return false;
 }
 return true;
}
/* Valid views only; conservatively includes row padding. */
static inline bool mulib_media_frame_disjoint(const mulib_media_frame *a,const mulib_media_frame *b){
 if(!mulib_media_frame_valid(a)||!mulib_media_frame_valid(b))return false;
 for(unsigned p=0;p<mulib_media_pixelformat_planes(a->format);p++){
  size_t an=0;mulib_media_frame_plane_extent(a,p,&an);
  for(unsigned q=0;q<mulib_media_pixelformat_planes(b->format);q++){
   size_t bn=0;mulib_media_frame_plane_extent(b,q,&bn);
   if(!mulib_media_ranges_disjoint(a->data[p],an,b->data[q],bn))return false;
  }
 }
 return true;
}
/* Owned frame must be initially zeroed. Release only frames allocated here. */
static inline void mulib_media_frame_release(mulib_media_frame *f){if(f){for(unsigned p=0;p<3;p++)free(f->data[p]);memset(f,0,sizeof *f);}}
static inline bool mulib_media_frame_alloc(mulib_media_frame *f,size_t w,size_t h,mulib_media_pixelformat fmt){if(!f||f->data[0]||f->data[1]||f->data[2]||!mulib_media_pixelformat_planes(fmt))return false;mulib_media_frame t={0};t.width=w;t.height=h;t.format=fmt;for(unsigned p=0;p<mulib_media_pixelformat_planes(fmt);p++){size_t n;if(!mulib_media_pixelformat_layout(fmt,w,h,p,&t.stride[p],&n)||!mulib_media_size_mul(n,t.stride[p],&t.capacity[p])||!(t.data[p]=(uint8_t*)calloc(1,t.capacity[p]))){mulib_media_frame_release(&t);return false;}}*f=t;return true;}
static inline bool mulib_media_frame_copy(mulib_media_frame *d,const mulib_media_frame *s){
 if(!mulib_media_frame_valid(d)||!mulib_media_frame_valid(s)||d->format!=s->format||d->width!=s->width||d->height!=s->height)return false;
 bool identical=true;
 for(unsigned p=0;p<mulib_media_pixelformat_planes(s->format);p++)if(d->data[p]!=s->data[p]||d->stride[p]!=s->stride[p])identical=false;
 if(!identical&&!mulib_media_frame_disjoint(d,s))return false;
 if(!identical)for(unsigned p=0;p<mulib_media_pixelformat_planes(s->format);p++){
  size_t row=0,n=0;mulib_media_pixelformat_layout(s->format,s->width,s->height,p,&row,&n);
  for(size_t y=0;y<n;y++)memcpy(d->data[p]+y*d->stride[p],s->data[p]+y*s->stride[p],row);
 }
 d->pts=s->pts;d->timebase=s->timebase;return true;
}
#ifdef __cplusplus
}
#endif
#endif
