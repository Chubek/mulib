/* MuLIB / Amb3r: pcb-spatial. C11 single-header kernel.
 * Define MULIB_IMPL_PCB_SPATIAL in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PCB_SPATIAL_H
#define MULIB_PCB_SPATIAL_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {ma_box box;size_t id;} ma_spatial_item;
typedef struct {ma_spatial_item *items;size_t n;} ma_spatial;
int mulib_pcb_spatial_build(ma_spatial *s,const ma_box *boxes,size_t n);
size_t mulib_pcb_spatial_query(const ma_spatial *s,ma_box b,size_t *out,size_t cap);
void mulib_pcb_spatial_free(ma_spatial *s);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PCB_SPATIAL) && !defined(MULIB_PCB_SPATIAL_H_IMPLEMENTED)
#define MULIB_PCB_SPATIAL_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_pcb_spatial_build(ma_spatial *s,const ma_box *boxes,size_t n){if(!s||s->items||(!boxes&&n))return 0;ma_spatial_item *v=(ma_spatial_item*)ma_alloc(n,sizeof *v);if(!v)return 0;for(size_t i=0;i<n;i++){if(!ma_box_valid(boxes[i])){free(v);return 0;}v[i]=(ma_spatial_item){boxes[i],i};size_t j=i;while(j&&v[j].box.lo.x<v[j-1].box.lo.x){ma_spatial_item t=v[j];v[j]=v[j-1];v[j-1]=t;j--;}}s->items=v;s->n=n;return 1;}
size_t mulib_pcb_spatial_query(const ma_spatial *s,ma_box b,size_t *out,size_t cap){if(!s||!ma_box_valid(b))return MA_NONE;size_t n=0;for(size_t i=0;i<s->n&&s->items[i].box.lo.x<=b.hi.x;i++)if(ma_box_hit(s->items[i].box,b)){if(out&&n<cap)out[n]=s->items[i].id;n++;}return n;}
void mulib_pcb_spatial_free(ma_spatial *s){if(s){free(s->items);memset(s,0,sizeof *s);}}
#ifdef __cplusplus
}
#endif
#endif
