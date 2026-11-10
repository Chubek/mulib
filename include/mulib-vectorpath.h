/** @file mulib-vectorpath.h
 * @brief Owned ordered collection for graphics vectorpath rectangles and records; insertion, indexing, removal.
 * ISO C11, no third-party libraries. Define MULIB_IMPL_VECTORPATH once.
 * Public implementation depends on mulib-typecore.h (MULIB_IMPL_TYPECORE).
 * See manual/mulib-vectorpath.md for scope and limitations.
 */
#ifndef MULIB_VECTORPATH_H
#define MULIB_VECTORPATH_H
#include "mulib-typecore.h"
#include <math.h>
#include <ctype.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct mulib_vectorpath_item { double x,y,width,height; unsigned flags; } mulib_vectorpath_item_t;
typedef struct mulib_vectorpath_list { mulib_vectorpath_item_t *items; size_t count,capacity; } mulib_vectorpath_list_t;
int mulib_vectorpath_push(mulib_vectorpath_list_t *list, mulib_vectorpath_item_t item);
int mulib_vectorpath_remove(mulib_vectorpath_list_t *list, size_t index);
int mulib_vectorpath_bounds(const mulib_vectorpath_list_t *list, double *width, double *height);
void mulib_vectorpath_destroy(mulib_vectorpath_list_t *list);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_VECTORPATH_H */
#if defined(MULIB_IMPL_VECTORPATH) && !defined(MULIB_VECTORPATH_H_IMPLEMENTED)
#define MULIB_VECTORPATH_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_vectorpath_push(mulib_vectorpath_list_t *l,mulib_vectorpath_item_t x){if(!l||!isfinite(x.x)||!isfinite(x.y)||!isfinite(x.width)||!isfinite(x.height)||x.width<0||x.height<0)return 0;
 if(l->count==l->capacity){size_t c=l->capacity?l->capacity*2:8;if(c<l->count||c>SIZE_MAX/sizeof(*l->items))return 0;void *p=realloc(l->items,c*sizeof(*l->items));if(!p)return 0;l->items=(mulib_vectorpath_item_t*)p;l->capacity=c;}
 l->items[l->count++]=x;return 1;}
int mulib_vectorpath_remove(mulib_vectorpath_list_t *l,size_t i){if(!l||i>=l->count)return 0;memmove(l->items+i,l->items+i+1,(l->count-i-1)*sizeof(*l->items));l->count--;return 1;}
int mulib_vectorpath_bounds(const mulib_vectorpath_list_t *l,double *w,double *h){if(!l||!w||!h)return 0;double x=0,y=0;for(size_t i=0;i<l->count;i++){double a=l->items[i].x+l->items[i].width,b=l->items[i].y+l->items[i].height;if(!isfinite(a)||!isfinite(b))return 0;if(a>x)x=a;if(b>y)y=b;}*w=x;*h=y;return 1;}
void mulib_vectorpath_destroy(mulib_vectorpath_list_t *l){if(l){free(l->items);memset(l,0,sizeof(*l));}}
#ifdef __cplusplus
}
#endif
#endif
