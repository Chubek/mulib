/* MuLIB: formal-sec. C11 single-header kernel.
 * Define MULIB_IMPL_FORMAL_SEC in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_FORMAL_SEC_H
#define MULIB_FORMAL_SEC_H
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

int mulib_formal_sec(size_t na,size_t nb,size_t alphabet,const size_t *a,const size_t *b,const uint64_t *oa,const uint64_t *ob,size_t init_a,size_t init_b,size_t *bad_a,size_t *bad_b);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_FORMAL_SEC) && !defined(MULIB_FORMAL_SEC_H_IMPLEMENTED)
#define MULIB_FORMAL_SEC_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_formal_sec(size_t na,size_t nb,size_t alphabet,const size_t *a,const size_t *b,const uint64_t *oa,const uint64_t *ob,size_t init_a,size_t init_b,size_t *bad_a,size_t *bad_b){size_t n,an,bn;if(!na||!nb||!alphabet||!mulib_storage_size_mul(na,nb,&n)||!mulib_storage_size_mul(na,alphabet,&an)||!mulib_storage_size_mul(nb,alphabet,&bn)||!a||!b||!oa||!ob||init_a>=na||init_b>=nb)return -1;for(size_t i=0;i<an;i++)if(a[i]>=na)return -1;for(size_t i=0;i<bn;i++)if(b[i]>=nb)return -1;unsigned char *seen=(unsigned char*)mulib_storage_alloc(n,1);size_t *q=(size_t*)mulib_storage_alloc(n,sizeof *q);if(!seen||!q){free(seen);free(q);return -1;}size_t head=0,tail=0,start=init_a*nb+init_b;q[tail++]=start;seen[start]=1;int r=1;while(head<tail){size_t x=q[head++],u=x/nb,v=x%nb;for(size_t k=0;k<alphabet;k++){if(oa[u*alphabet+k]!=ob[v*alphabet+k]){if(bad_a)*bad_a=u;if(bad_b)*bad_b=v;r=0;goto end;}size_t z=a[u*alphabet+k]*nb+b[v*alphabet+k];if(!seen[z]){seen[z]=1;q[tail++]=z;}}}end:free(seen);free(q);return r;}
#ifdef __cplusplus
}
#endif
#endif
