/** @file mulib-cdd-catalogue.h
 * @brief Immutable catalogue views, versioned capabilities, typed metadata and structural validation.
 * ISO C11; define MULIB_IMPL_CDD_CATALOGUE in exactly one translation unit.
 * See manual/mulib-cdd-catalogue.md for ownership and error contracts.
 */
#ifndef MULIB_CDD_CATALOGUE_H
#define MULIB_CDD_CATALOGUE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MULIB_CDD_MAX_ENTRIES 64u
#define MULIB_CDD_NONE ((size_t)-1)
typedef enum {
    MULIB_CDD_OK=0, MULIB_CDD_INVALID=-1, MULIB_CDD_DUPLICATE=-2,
    MULIB_CDD_NOT_FOUND=-3, MULIB_CDD_CONFLICT=-4, MULIB_CDD_UNSAT=-5,
    MULIB_CDD_LIMIT=-6, MULIB_CDD_NOMEM=-7, MULIB_CDD_CALLBACK=-8
} mulib_cdd_status_t;
typedef enum { MULIB_CDD_INT=1, MULIB_CDD_BOOL, MULIB_CDD_STRING } mulib_cdd_kind_t;
typedef struct { mulib_cdd_kind_t kind; int64_t integer; const char *string; } mulib_cdd_value_t;
typedef struct { const char *key; mulib_cdd_value_t value; } mulib_cdd_attr_t;
typedef struct { const char *name; uint32_t revision; } mulib_cdd_cap_t;
/* Inclusive range; max_revision == 0 means unbounded. Revisions start at 1. */
typedef struct { const char *name; uint32_t min_revision, max_revision; } mulib_cdd_req_t;
typedef struct {
    const char *id, *category, *implementation;
    uint32_t revision, cost;
    const mulib_cdd_cap_t *provides; size_t provide_count;
    const mulib_cdd_req_t *requirements; size_t require_count;
    const char *const *conflicts; size_t conflict_count;
    const mulib_cdd_attr_t *attrs; size_t attr_count;
} mulib_cdd_entry_t;
typedef struct { const mulib_cdd_entry_t *entries; size_t count; } mulib_cdd_catalogue_t;
typedef struct { mulib_cdd_status_t status; size_t entry, item; const char *field; } mulib_cdd_diag_t;
int mulib_cdd_name_valid(const char *s);
int mulib_cdd_value_valid(const mulib_cdd_value_t *v);
int mulib_cdd_req_valid(const mulib_cdd_req_t *r);
int mulib_cdd_cap_matches(const mulib_cdd_cap_t *c, const mulib_cdd_req_t *r);
int mulib_cdd_entry_provides(const mulib_cdd_entry_t *e, const mulib_cdd_req_t *r);
int mulib_cdd_entries_conflict(const mulib_cdd_entry_t *a, const mulib_cdd_entry_t *b);
const mulib_cdd_attr_t *mulib_cdd_attr_get(const mulib_cdd_entry_t *e, const char *key);
size_t mulib_cdd_find(const mulib_cdd_catalogue_t *c, const char *id);
mulib_cdd_status_t mulib_cdd_validate(const mulib_cdd_catalogue_t *c, mulib_cdd_diag_t *diag);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CDD_CATALOGUE_H */
#if defined(MULIB_IMPL_CDD_CATALOGUE) && !defined(MULIB_CDD_CATALOGUE_IMPLEMENTED)
#define MULIB_CDD_CATALOGUE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_cdd_name_valid(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; ++s) {
        unsigned char ch=(unsigned char)*s;
        if (!((ch>='a' && ch<='z') || (ch>='A' && ch<='Z') ||
              (ch>='0' && ch<='9') || ch=='_' || ch=='-' || ch=='.' || ch==':')) return 0;
    }
    return 1;
}
int mulib_cdd_value_valid(const mulib_cdd_value_t *v) {
    return v && (v->kind==MULIB_CDD_INT || (v->kind==MULIB_CDD_BOOL &&
        (v->integer==0 || v->integer==1)) || (v->kind==MULIB_CDD_STRING && v->string));
}
int mulib_cdd_req_valid(const mulib_cdd_req_t *r) {
    return r && mulib_cdd_name_valid(r->name) && r->min_revision &&
        (!r->max_revision || r->max_revision>=r->min_revision);
}
int mulib_cdd_cap_matches(const mulib_cdd_cap_t *c,const mulib_cdd_req_t *r) {
    return c && c->name && mulib_cdd_req_valid(r) && !strcmp(c->name,r->name) &&
        c->revision>=r->min_revision && (!r->max_revision || c->revision<=r->max_revision);
}
int mulib_cdd_entry_provides(const mulib_cdd_entry_t *e,const mulib_cdd_req_t *r) {
    if (!e || (!e->provides && e->provide_count)) return 0;
    for (size_t i=0;i<e->provide_count;i++) if (mulib_cdd_cap_matches(e->provides+i,r)) return 1;
    return 0;
}
int mulib_cdd_entries_conflict(const mulib_cdd_entry_t *a,const mulib_cdd_entry_t *b) {
    if (!a || !b || !a->id || !b->id) return 0;
    for (size_t i=0;i<a->conflict_count;i++) if (!strcmp(a->conflicts[i],b->id)) return 1;
    for (size_t i=0;i<b->conflict_count;i++) if (!strcmp(b->conflicts[i],a->id)) return 1;
    return 0;
}
const mulib_cdd_attr_t *mulib_cdd_attr_get(const mulib_cdd_entry_t *e,const char *key) {
    if (!e || !key || (!e->attrs && e->attr_count)) return NULL;
    for (size_t i=0;i<e->attr_count;i++) if (e->attrs[i].key && !strcmp(e->attrs[i].key,key)) return e->attrs+i;
    return NULL;
}
size_t mulib_cdd_find(const mulib_cdd_catalogue_t *c,const char *id) {
    if (!c || !id || !c->entries || c->count>MULIB_CDD_MAX_ENTRIES) return MULIB_CDD_NONE;
    for (size_t i=0;i<c->count;i++) if (c->entries[i].id && !strcmp(c->entries[i].id,id)) return i;
    return MULIB_CDD_NONE;
}
static mulib_cdd_status_t mulib_cdd_diag_(mulib_cdd_diag_t *d,mulib_cdd_status_t s,size_t i,size_t j,const char *f) {
    if (d) { d->status=s; d->entry=i; d->item=j; d->field=f; } return s;
}
mulib_cdd_status_t mulib_cdd_validate(const mulib_cdd_catalogue_t *c,mulib_cdd_diag_t *d) {
    if (!c || (!c->entries && c->count)) return mulib_cdd_diag_(d,MULIB_CDD_INVALID,MULIB_CDD_NONE,MULIB_CDD_NONE,"catalogue");
    if (c->count>MULIB_CDD_MAX_ENTRIES) return mulib_cdd_diag_(d,MULIB_CDD_LIMIT,MULIB_CDD_NONE,MULIB_CDD_NONE,"count");
    for (size_t i=0;i<c->count;i++) {
        const mulib_cdd_entry_t *e=c->entries+i;
        if (!mulib_cdd_name_valid(e->id) || !mulib_cdd_name_valid(e->category) || !e->implementation || !*e->implementation || !e->revision)
            return mulib_cdd_diag_(d,MULIB_CDD_INVALID,i,MULIB_CDD_NONE,"identity");
        for (size_t j=0;j<i;j++) if (!strcmp(e->id,c->entries[j].id)) return mulib_cdd_diag_(d,MULIB_CDD_DUPLICATE,i,j,"id");
        if ((!e->provides && e->provide_count) || (!e->requirements && e->require_count) || (!e->conflicts && e->conflict_count) || (!e->attrs && e->attr_count))
            return mulib_cdd_diag_(d,MULIB_CDD_INVALID,i,MULIB_CDD_NONE,"array");
        for (size_t j=0;j<e->provide_count;j++) {
            if (!mulib_cdd_name_valid(e->provides[j].name) || !e->provides[j].revision) return mulib_cdd_diag_(d,MULIB_CDD_INVALID,i,j,"provides");
            for (size_t k=0;k<j;k++) if (!strcmp(e->provides[j].name,e->provides[k].name) && e->provides[j].revision==e->provides[k].revision) return mulib_cdd_diag_(d,MULIB_CDD_DUPLICATE,i,j,"provides");
        }
        for (size_t j=0;j<e->require_count;j++) if (!mulib_cdd_req_valid(e->requirements+j)) return mulib_cdd_diag_(d,MULIB_CDD_INVALID,i,j,"requirements");
        for (size_t j=0;j<e->conflict_count;j++) {
            if (!mulib_cdd_name_valid(e->conflicts[j]) || !strcmp(e->id,e->conflicts[j])) return mulib_cdd_diag_(d,MULIB_CDD_INVALID,i,j,"conflicts");
            for (size_t k=0;k<j;k++) if (!strcmp(e->conflicts[j],e->conflicts[k])) return mulib_cdd_diag_(d,MULIB_CDD_DUPLICATE,i,j,"conflicts");
        }
        for (size_t j=0;j<e->attr_count;j++) {
            if (!mulib_cdd_name_valid(e->attrs[j].key) || !mulib_cdd_value_valid(&e->attrs[j].value)) return mulib_cdd_diag_(d,MULIB_CDD_INVALID,i,j,"attrs");
            for (size_t k=0;k<j;k++) if (!strcmp(e->attrs[j].key,e->attrs[k].key)) return mulib_cdd_diag_(d,MULIB_CDD_DUPLICATE,i,j,"attrs");
        }
    }
    return mulib_cdd_diag_(d,MULIB_CDD_OK,MULIB_CDD_NONE,MULIB_CDD_NONE,NULL);
}
#ifdef __cplusplus
}
#endif
#endif
