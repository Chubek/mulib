/* MuLIB / Amb3r: eda-revision. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_REVISION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_REVISION_H
#define MULIB_EDA_REVISION_H
#include "mulib-amb3r-base.h"
#include "mulib-eda-store.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t number;ma_design_store snapshot;} ma_revision;
int mulib_eda_revision_capture(const ma_design_store *store,ma_revision *r);
void mulib_eda_revision_free(ma_revision *r);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_REVISION) && !defined(MULIB_EDA_REVISION_H_IMPLEMENTED)
#define MULIB_EDA_REVISION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_revision_capture(const ma_design_store *store,ma_revision *r){if(!r||r->snapshot.v)return 0;if(!mulib_eda_store_clone(store,&r->snapshot))return 0;r->number=store->revision;return 1;}
void mulib_eda_revision_free(ma_revision *r){if(r){mulib_eda_store_free(&r->snapshot);r->number=0;}}
#ifdef __cplusplus
}
#endif
#endif
