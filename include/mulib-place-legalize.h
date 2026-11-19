/* MuLIB / Amb3r: place-legalize. C11 single-header kernel.
 * Define MULIB_IMPL_PLACE_LEGALIZE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_PLACE_LEGALIZE_H
#define MULIB_PLACE_LEGALIZE_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_place_legalize(const ma_point *desired,size_t n,const ma_point *sites,size_t m,const unsigned char *blocked,size_t *assigned);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_PLACE_LEGALIZE) && !defined(MULIB_PLACE_LEGALIZE_H_IMPLEMENTED)
#define MULIB_PLACE_LEGALIZE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_place_legalize(const ma_point *desired,size_t n,const ma_point *sites,size_t m,const unsigned char *blocked,size_t *assigned){if((!desired&&n)||(!sites&&m)||(!assigned&&n)||n>m)return 0;unsigned char *used=(unsigned char*)ma_alloc(m,1);if(!used)return 0;if(blocked)memcpy(used,blocked,m);for(size_t i=0;i<n;i++){size_t best=MA_NONE;double d=INFINITY;for(size_t j=0;j<m;j++)if(!used[j]){double q=ma_dist(desired[i],sites[j]);if(q<d){d=q;best=j;}}if(best==MA_NONE){free(used);return 0;}used[best]=1;assigned[i]=best;}free(used);return 1;}
#ifdef __cplusplus
}
#endif
#endif
