/** @file mulib-cdd-registry.h
 * @brief Bounded mutable registry with borrowed immutable descriptor payloads.
 * ISO C11; define MULIB_IMPL_CDD_REGISTRY in exactly one translation unit.
 * See manual/mulib-cdd-registry.md for ownership and error contracts.
 */
#ifndef MULIB_CDD_REGISTRY_H
#define MULIB_CDD_REGISTRY_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-cdd-catalogue.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { mulib_cdd_entry_t entries[MULIB_CDD_MAX_ENTRIES]; size_t count; } mulib_cdd_registry_t;
void mulib_cdd_registry_init(mulib_cdd_registry_t *r);
mulib_cdd_catalogue_t mulib_cdd_registry_view(const mulib_cdd_registry_t *r);
mulib_cdd_status_t mulib_cdd_registry_add(mulib_cdd_registry_t *r,const mulib_cdd_entry_t *e);
mulib_cdd_status_t mulib_cdd_registry_remove(mulib_cdd_registry_t *r,const char *id);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CDD_REGISTRY_H */
#if defined(MULIB_IMPL_CDD_REGISTRY) && !defined(MULIB_CDD_REGISTRY_IMPLEMENTED)
#define MULIB_CDD_REGISTRY_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
void mulib_cdd_registry_init(mulib_cdd_registry_t *r) { if (r) r->count=0; }
mulib_cdd_catalogue_t mulib_cdd_registry_view(const mulib_cdd_registry_t *r) {
    mulib_cdd_catalogue_t c={r?r->entries:NULL,r?r->count:0}; return c;
}
mulib_cdd_status_t mulib_cdd_registry_add(mulib_cdd_registry_t *r,const mulib_cdd_entry_t *e) {
    if (!r || !e) return MULIB_CDD_INVALID;
    if (r->count>=MULIB_CDD_MAX_ENTRIES) return MULIB_CDD_LIMIT;
    r->entries[r->count]=*e;
    mulib_cdd_catalogue_t c={r->entries,r->count+1};
    mulib_cdd_status_t s=mulib_cdd_validate(&c,NULL);
    if (s==MULIB_CDD_OK) r->count++;
    return s;
}
mulib_cdd_status_t mulib_cdd_registry_remove(mulib_cdd_registry_t *r,const char *id) {
    if (!r || !id || r->count>MULIB_CDD_MAX_ENTRIES) return MULIB_CDD_INVALID;
    mulib_cdd_catalogue_t c=mulib_cdd_registry_view(r);
    size_t i=mulib_cdd_find(&c,id);
    if (i==MULIB_CDD_NONE) return MULIB_CDD_NOT_FOUND;
    memmove(r->entries+i,r->entries+i+1,(r->count-i-1)*sizeof(*r->entries));
    r->count--; return MULIB_CDD_OK;
}
#ifdef __cplusplus
}
#endif
#endif
