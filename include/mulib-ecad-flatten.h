/* MuLIB / Amb3r: ecad-flatten. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_FLATTEN in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_FLATTEN_H
#define MULIB_ECAD_FLATTEN_H
#include "mulib-amb3r-base.h"
#include "mulib-ecad-sheet.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_ecad_flatten_paths(const ma_sheet *s,size_t n,size_t index,char *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_FLATTEN) && !defined(MULIB_ECAD_FLATTEN_H_IMPLEMENTED)
#define MULIB_ECAD_FLATTEN_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_flatten_paths(const ma_sheet *s,size_t n,size_t index,char *out,size_t cap){if(!s||index>=n||!out||!cap)return 0;size_t *chain=(size_t*)ma_alloc(n,sizeof(size_t));if(!chain)return 0;size_t k=0,i=index;while(1){if(k==n||!s[i].name||strchr(s[i].name,'/')){free(chain);return 0;}chain[k++]=i;if(!s[i].parent)break;uint64_t parent=s[i].parent;for(i=0;i<n&&s[i].id!=parent;i++){}if(i==n){free(chain);return 0;}}size_t pos=0;while(k){const char *name=s[chain[--k]].name;size_t len=strlen(name);if(len>=cap-pos-1){free(chain);return 0;}out[pos++]='/';memcpy(out+pos,name,len);pos+=len;}out[pos]=0;free(chain);return 1;}
#ifdef __cplusplus
}
#endif
#endif
