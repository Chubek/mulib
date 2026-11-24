/* MuLIB compiler tools: Patched byte KMP search with callback-driven enumeration of overlapping occurrences.
 * ISO C11 single-header module. Define MULIB_IMPL_SEARCH in ONE translation unit.
 * Bounded operations report failure instead of silently truncating data.
 */
#ifndef MULIB_SEARCH_H
#define MULIB_SEARCH_H
#include <stddef.h>
#include <stdint.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif
size_t mulib_search_kmp(const void *haystack,size_t hay_len,const void *needle,size_t needle_len);
typedef int (*mulib_search_match_fn)(size_t offset,void *user);
/* Counts emitted matches; SIZE_MAX invalid or OOM. Empty needle emits offset 0. */
size_t mulib_search_all(const void *haystack,size_t hay_len,const void *needle,size_t needle_len,mulib_search_match_fn visitor,void *user);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_SEARCH_H */
#if defined(MULIB_IMPL_SEARCH) && !defined(MULIB_SEARCH_IMPLEMENTED)
#define MULIB_SEARCH_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <math.h>
#ifdef __cplusplus
extern "C" {
#endif
static size_t *mulib_search_failure_(const unsigned char *p,size_t n){if(n>SIZE_MAX/sizeof(size_t))return NULL;
size_t *fail=(size_t*)calloc(n?n:1,sizeof(size_t));
if(!fail)return NULL;
 for(size_t i=1,j=0;i<n;i++){while(j&&p[i]!=p[j])j=fail[j-1];
if(p[i]==p[j])j++;
fail[i]=j;
}return fail;
}
size_t mulib_search_kmp(const void *hay,size_t hn,const void *needle,size_t nn){
 if(!nn)return 0;
if(!hay||!needle||nn>hn)return SIZE_MAX;
 const unsigned char *h=(const unsigned char*)hay,*p=(const unsigned char*)needle;
size_t *fail=mulib_search_failure_(p,nn);
if(!fail)return SIZE_MAX;
 size_t found=SIZE_MAX;
for(size_t i=0,j=0;i<hn;i++){while(j&&h[i]!=p[j])j=fail[j-1];
if(h[i]==p[j])j++;
if(j==nn){found=i+1-nn;
break;
}}
 free(fail);
return found;
}
size_t mulib_search_all(const void *hay,size_t hn,const void *needle,size_t nn,mulib_search_match_fn visit,void *user){
 if(!visit||(!hay&&hn)||(!needle&&nn))return SIZE_MAX;
if(!nn){visit(0,user);
return 1;
}if(nn>hn)return 0;
 const unsigned char *h=(const unsigned char*)hay,*p=(const unsigned char*)needle;
size_t *fail=mulib_search_failure_(p,nn);
if(!fail)return SIZE_MAX;
 size_t total=0;
for(size_t i=0,j=0;i<hn;i++){while(j&&h[i]!=p[j])j=fail[j-1];
if(h[i]==p[j])j++;
if(j==nn){total++;
int keep=visit(i+1-nn,user);
if(!keep)break;
j=fail[j-1];
}}
 free(fail);
return total;
}
#ifdef __cplusplus
}
#endif
#endif
