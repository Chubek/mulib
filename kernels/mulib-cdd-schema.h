/** @file mulib-cdd-schema.h
 * @brief Category-aware metadata schemas with required attributes and integer bounds.
 * ISO C11; define MULIB_IMPL_CDD_SCHEMA in exactly one translation unit.
 * See manual/mulib-cdd-schema.md for ownership and error contracts.
 */
#ifndef MULIB_CDD_SCHEMA_H
#define MULIB_CDD_SCHEMA_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-cdd-catalogue.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { const char *key; mulib_cdd_kind_t kind; int required, bounded; int64_t min,max; } mulib_cdd_field_t;
typedef struct { const char *category; const mulib_cdd_field_t *fields; size_t field_count; int closed; } mulib_cdd_schema_t;
/* Every matching entry is validated. closed rejects attributes not listed. */
mulib_cdd_status_t mulib_cdd_schema_validate(const mulib_cdd_catalogue_t *c,const mulib_cdd_schema_t *s,mulib_cdd_diag_t *diag);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CDD_SCHEMA_H */
#if defined(MULIB_IMPL_CDD_SCHEMA) && !defined(MULIB_CDD_SCHEMA_IMPLEMENTED)
#define MULIB_CDD_SCHEMA_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
static mulib_cdd_status_t mulib_cdd_schema_diag_(mulib_cdd_diag_t *d,mulib_cdd_status_t status,size_t i,size_t j,const char *field) {
    if (d) { d->status=status; d->entry=i; d->item=j; d->field=field; } return status;
}
mulib_cdd_status_t mulib_cdd_schema_validate(const mulib_cdd_catalogue_t *c,const mulib_cdd_schema_t *s,mulib_cdd_diag_t *d) {
    mulib_cdd_status_t status=mulib_cdd_validate(c,d); if (status) return status;
    if (!s || (s->category && !mulib_cdd_name_valid(s->category)) || (!s->fields && s->field_count)) return mulib_cdd_schema_diag_(d,MULIB_CDD_INVALID,MULIB_CDD_NONE,MULIB_CDD_NONE,"schema");
    for (size_t j=0;j<s->field_count;j++) {
        const mulib_cdd_field_t *f=s->fields+j;
        if (!mulib_cdd_name_valid(f->key) || f->kind<MULIB_CDD_INT || f->kind>MULIB_CDD_STRING || (f->bounded && (f->kind!=MULIB_CDD_INT || f->min>f->max)))
            return mulib_cdd_schema_diag_(d,MULIB_CDD_INVALID,MULIB_CDD_NONE,j,"schema.field");
        for (size_t k=0;k<j;k++) if (!strcmp(f->key,s->fields[k].key)) return mulib_cdd_schema_diag_(d,MULIB_CDD_DUPLICATE,MULIB_CDD_NONE,j,"schema.field");
    }
    for (size_t i=0;i<c->count;i++) {
        const mulib_cdd_entry_t *e=c->entries+i;
        if (s->category && strcmp(s->category,e->category)) continue;
        for (size_t j=0;j<s->field_count;j++) {
            const mulib_cdd_field_t *f=s->fields+j;
            const mulib_cdd_attr_t *a=mulib_cdd_attr_get(e,f->key);
            if (!a) { if (f->required) return mulib_cdd_schema_diag_(d,MULIB_CDD_NOT_FOUND,i,j,f->key); continue; }
            if (a->value.kind!=f->kind || (f->bounded && (a->value.integer<f->min || a->value.integer>f->max))) return mulib_cdd_schema_diag_(d,MULIB_CDD_INVALID,i,j,f->key);
        }
        if (s->closed) for (size_t j=0;j<e->attr_count;j++) {
            size_t k; for (k=0;k<s->field_count;k++) if (!strcmp(e->attrs[j].key,s->fields[k].key)) break;
            if (k==s->field_count) return mulib_cdd_schema_diag_(d,MULIB_CDD_INVALID,i,j,e->attrs[j].key);
        }
    }
    return MULIB_CDD_OK;
}
#ifdef __cplusplus
}
#endif
#endif
