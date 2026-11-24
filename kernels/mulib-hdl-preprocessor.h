/* MuLIB: hdl-preprocessor. C11 single-header kernel.
 * Define MULIB_IMPL_HDL_PREPROCESSOR in exactly one implementation translation unit.
 * Profile: Single-pass object-like backtick substitution; no full HDL preprocessor.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_HDL_PREPROCESSOR_H
#define MULIB_HDL_PREPROCESSOR_H
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
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {const char *name,*value;} mulib_hdl_macro;
int mulib_hdl_preprocessor_expand(const char *s,const mulib_hdl_macro *macros,size_t n,mulib_storage_bytes *out);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_HDL_PREPROCESSOR) && !defined(MULIB_HDL_PREPROCESSOR_H_IMPLEMENTED)
#define MULIB_HDL_PREPROCESSOR_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_hdl_preprocessor_expand(const char *s,const mulib_hdl_macro *macros,size_t n,mulib_storage_bytes *out){if(!s||(!macros&&n)||!out||out->v)return 0;mulib_storage_bytes b={0};for(size_t i=0;s[i];){if(s[i]!='`'){if(!mulib_storage_bytes_append(&b,s+i,1))goto fail;i++;continue;}size_t start=++i;while(isalnum((unsigned char)s[i])||s[i]=='_')i++;if(i==start)goto fail;size_t j=0;for(;j<n;j++)if(macros[j].name&&strlen(macros[j].name)==i-start&&!memcmp(s+start,macros[j].name,i-start))break;if(j==n||!macros[j].value||!mulib_storage_bytes_append(&b,macros[j].value,strlen(macros[j].value)))goto fail;}if(!mulib_storage_bytes_append(&b,"",1))goto fail;b.n--;*out=b;return 1;fail:mulib_storage_bytes_free(&b);return 0;}
#ifdef __cplusplus
}
#endif
#endif
