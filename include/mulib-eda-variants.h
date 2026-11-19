/* MuLIB / Amb3r: eda-variants. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_VARIANTS in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_VARIANTS_H
#define MULIB_EDA_VARIANTS_H
#include "mulib-amb3r-base.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint32_t variant;uint64_t component,replacement;int populate;} ma_variant_entry;
int mulib_eda_variants_resolve(const ma_variant_entry *v,size_t n,uint32_t variant,uint64_t component,uint64_t *replacement,int *populate);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_VARIANTS) && !defined(MULIB_EDA_VARIANTS_H_IMPLEMENTED)
#define MULIB_EDA_VARIANTS_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_variants_resolve(const ma_variant_entry *v,size_t n,uint32_t variant,uint64_t component,uint64_t *replacement,int *populate){if((!v&&n)||!component||!replacement||!populate)return 0;*replacement=component;*populate=1;for(size_t i=0;i<n;i++)if(v[i].variant==variant&&v[i].component==component){*replacement=v[i].replacement?v[i].replacement:component;*populate=!!v[i].populate;return 1;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
