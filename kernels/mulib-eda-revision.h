/* MuLIB: eda-revision. C11 single-header kernel.
 * Define MULIB_IMPL_EDA_REVISION in exactly one implementation translation unit.
 * Unless stated otherwise, 1 indicates success, 0 indicates failure.
 */
#ifndef MULIB_EDA_REVISION_H
#define MULIB_EDA_REVISION_H
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
#include "mulib-eda-store.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {uint64_t number;mulib_eda_design_store snapshot;} mulib_eda_revision;
int mulib_eda_revision_capture(const mulib_eda_design_store *store,mulib_eda_revision *r);
void mulib_eda_revision_free(mulib_eda_revision *r);
#ifdef __cplusplus
}
#endif
#endif
#if defined(MULIB_IMPL_EDA_REVISION) && !defined(MULIB_EDA_REVISION_H_IMPLEMENTED)
#define MULIB_EDA_REVISION_H_IMPLEMENTED
#ifdef __cplusplus
extern "C" {
#endif
int mulib_eda_revision_capture(const mulib_eda_design_store *store,mulib_eda_revision *r){if(!r||r->snapshot.v)return 0;if(!mulib_eda_store_clone(store,&r->snapshot))return 0;r->number=store->revision;return 1;}
void mulib_eda_revision_free(mulib_eda_revision *r){if(r){mulib_eda_store_free(&r->snapshot);r->number=0;}}
#ifdef __cplusplus
}
#endif
#endif
