/* MuLIB / Amb3r: eda-validate. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_VALIDATE in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_VALIDATE_H
#define MULIB_EDA_VALIDATE_H
#include "mulib-amb3r-base.h"
#include "mulib-eda-store.h"
#ifdef __cplusplus
extern "C" {
#endif

int mulib_eda_validate(const ma_design_store *s,uint64_t *bad);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_VALIDATE) && !defined(MULIB_EDA_VALIDATE_H_IMPLEMENTED)
#define MULIB_EDA_VALIDATE_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_validate(const ma_design_store *s,uint64_t *bad){if(!s||s->n>s->cap||(s->n&&!s->v))return 0;for(size_t i=0;i<s->n;i++){ma_design_record r=s->v[i];if(!r.id||!r.kind||!isfinite(r.value)||!ma_point_valid(r.position)||mulib_eda_store_find(s,r.id)!=i)goto fail;uint64_t p=r.parent;for(size_t k=0;p;k++){size_t j=mulib_eda_store_find(s,p);if(j==MA_NONE||k>=s->n||p==r.id)goto fail;p=s->v[j].parent;}continue;fail:if(bad)*bad=r.id;return 0;}return 1;}
#ifdef __cplusplus
}
#endif
#endif
