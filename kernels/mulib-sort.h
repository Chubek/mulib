/* MuLIB compiler tools: Patched generic sorting with additional stable merge sort and lower-bound lookup.
 * ISO C11 single-header module. Define MULIB_IMPL_SORT in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_SORT_H
#define MULIB_SORT_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
void mulib_sort_array(void *base,size_t count,size_t stride,int (*compare)(const void *,const void *));
void *mulib_sort_binary(const void *key,const void *base,size_t count,size_t stride,int (*compare)(const void *,const void *));
/* Stable O(n log n), 1 success, 0 invalid or allocation failure. */
int mulib_sort_stable(void *base,size_t count,size_t stride,int (*compare)(const void *,const void *));
/* Index of first element not less than key; SIZE_MAX invalid. */
size_t mulib_sort_lower_bound(const void *key,const void *base,size_t count,size_t stride,int (*compare)(const void *,const void *));
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SORT_H */
#if defined(MULIB_IMPL_SORT) && !defined(MULIB_SORT_IMPLEMENTED)
#define MULIB_SORT_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_sort_array(void *p,size_t n,size_t z,int (*cmp)(const void *,const void *)){if(p&&z&&cmp&&n<=SIZE_MAX/z)qsort(p,n,z,cmp);
}
void *mulib_sort_binary(const void *k,const void *p,size_t n,size_t z,int (*cmp)(const void *,const void *)){return k&&p&&z&&cmp&&n<=SIZE_MAX/z?bsearch(k,p,n,z,cmp):NULL;
}
size_t mulib_sort_lower_bound(const void *key,const void *base,size_t n,size_t z,int (*cmp)(const void *,const void *)){
 if(!key||(!base&&n)||!z||!cmp||n>SIZE_MAX/z)return SIZE_MAX;
const unsigned char *p=(const unsigned char*)base;
size_t lo=0,hi=n;
 while(lo<hi){size_t mid=lo+(hi-lo)/2;
if(cmp(key,p+mid*z)>0)lo=mid+1;
else hi=mid;
}return lo;
}
int mulib_sort_stable(void *base,size_t count,size_t width,int (*cmp)(const void *,const void *)){
 if((!base&&count)||!width||!cmp||count>SIZE_MAX/width)return 0;
if(count<2)return 1;
 size_t total=count*width;
unsigned char *temp=(unsigned char*)malloc(total);
if(!temp)return 0;
 unsigned char *src=(unsigned char*)base,*dst=temp;
size_t step=1;
 for(;;){for(size_t lo=0;lo<count;){size_t mid=lo+(step<count-lo?step:count-lo);
size_t hi=mid+(step<count-mid?step:count-mid);
size_t a=lo,b=mid;
 for(size_t k=lo;k<hi;k++){if(a<mid&&(b==hi||cmp(src+a*width,src+b*width)<=0)){memcpy(dst+k*width,src+a*width,width);
a++;
}else{memcpy(dst+k*width,src+b*width,width);
b++;
}}lo=hi;
}
 unsigned char *x=src;
src=dst;
dst=x;
if(step>=count-step)break;
step*=2;
}
 if(src!=(unsigned char*)base)memcpy(base,src,total);
free(temp);
return 1;
}
#ifdef __cplusplus
}
#endif
#endif
