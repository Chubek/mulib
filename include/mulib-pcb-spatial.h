/* MuLIB: pcb-spatial. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_SPATIAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_SPATIAL_H
#define MULIB_PCB_SPATIAL_H
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
#include "mulib-geom2-core.h"
#include "mulib-graph-core.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {mulib_geom2_box box;size_t id;} mulib_pcb_spatial_item;
typedef struct {mulib_pcb_spatial_item *items;size_t n;} mulib_pcb_spatial;
int mulib_pcb_spatial_build(mulib_pcb_spatial *s,const mulib_geom2_box *boxes,size_t n);
size_t mulib_pcb_spatial_query(const mulib_pcb_spatial *s,mulib_geom2_box b,size_t *out,size_t cap);
void mulib_pcb_spatial_free(mulib_pcb_spatial *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_SPATIAL) && !defined(MULIB_PCB_SPATIAL_H_IMPLEMENTED)
#define MULIB_PCB_SPATIAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_spatial_build(mulib_pcb_spatial *s,const mulib_geom2_box *boxes,size_t n){if(!s||s->items||(!boxes&&n))return 0;mulib_pcb_spatial_item *v=(mulib_pcb_spatial_item*)mulib_storage_alloc(n,sizeof *v);if(!v)return 0;for(size_t i=0;i<n;i++){if(!mulib_geom2_box_valid(boxes[i])){free(v);return 0;}v[i]=(mulib_pcb_spatial_item){boxes[i],i};size_t j=i;while(j&&v[j].box.lo.x<v[j-1].box.lo.x){mulib_pcb_spatial_item t=v[j];v[j]=v[j-1];v[j-1]=t;j--;}}s->items=v;s->n=n;return 1;}
size_t mulib_pcb_spatial_query(const mulib_pcb_spatial *s,mulib_geom2_box b,size_t *out,size_t cap){if(!s||!mulib_geom2_box_valid(b))return MULIB_STORAGE_NONE;size_t n=0;for(size_t i=0;i<s->n&&s->items[i].box.lo.x<=b.hi.x;i++)if(mulib_geom2_box_overlap(s->items[i].box,b)){if(out&&n<cap)out[n]=s->items[i].id;n++;}return n;}
void mulib_pcb_spatial_free(mulib_pcb_spatial *s){if(s){free(s->items);memset(s,0,sizeof *s);}}
#ifdef __cplusplus
}
#endif
#endif
