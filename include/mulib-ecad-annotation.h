/* MuLIB / Amb3r: ecad-annotation. C11 single-header kernel.
 * Define MULIB_IMPL_ECAD_ANNOTATION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_ECAD_ANNOTATION_H
#define MULIB_ECAD_ANNOTATION_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_ecad_annotation_next(const char *prefix,const char *const *used,size_t n,char *out,size_t cap);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_ECAD_ANNOTATION) && !defined(MULIB_ECAD_ANNOTATION_H_IMPLEMENTED)
#define MULIB_ECAD_ANNOTATION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_ecad_annotation_next(const char *prefix,const char *const *used,size_t n,char *out,size_t cap){if(!prefix||!*prefix||(!used&&n)||!out||!cap)return 0;for(size_t i=1;i<=n+1&&i;i++){int k=snprintf(out,cap,"%s%zu",prefix,i);if(k<0||(size_t)k>=cap)return 0;size_t j=0;for(;j<n;j++)if(used[j]&&!strcmp(out,used[j]))break;if(j==n)return 1;}return 0;}
#ifdef __cplusplus
}
#endif
#endif
