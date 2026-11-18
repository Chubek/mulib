/** @file mulib-cdd-plan.h
 * @brief Problem descriptors, ordered plans and independent validation.
 * ISO C11; define MULIB_IMPL_CDD_PLAN in exactly one translation unit.
 * See manual/mulib-cdd-plan.md for ownership and error contracts.
 */
#ifndef MULIB_CDD_PLAN_H
#define MULIB_CDD_PLAN_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-cdd-catalogue.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    const mulib_cdd_req_t *goals; size_t goal_count;
    const mulib_cdd_cap_t *available; size_t available_count;
    uint64_t allowed; /* bit i enables catalogue entry i; UINT64_MAX enables all */
    uint64_t max_cost;
} mulib_cdd_problem_t;
typedef struct { size_t order[MULIB_CDD_MAX_ENTRIES]; size_t count; uint64_t cost; int found; } mulib_cdd_plan_t;
int mulib_cdd_problem_valid(const mulib_cdd_problem_t *p);
/* Following predicates require validated descriptors and a valid selected set.
 * True if a revision-compatible external or already selected provider exists. */
int mulib_cdd_satisfied(const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p,uint64_t selected,const mulib_cdd_req_t *r);
/* True if entry can be appended (allowed, unique, conflict-free, prerequisites met). */
int mulib_cdd_can_append(const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p,uint64_t selected,size_t index);
mulib_cdd_status_t mulib_cdd_plan_validate(const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p,const mulib_cdd_plan_t *plan,size_t *failed);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CDD_PLAN_H */
#if defined(MULIB_IMPL_CDD_PLAN) && !defined(MULIB_CDD_PLAN_IMPLEMENTED)
#define MULIB_CDD_PLAN_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
int mulib_cdd_problem_valid(const mulib_cdd_problem_t *p) {
    if (!p || (!p->goals && p->goal_count) || (!p->available && p->available_count)) return 0;
    for (size_t i=0;i<p->goal_count;i++) if (!mulib_cdd_req_valid(p->goals+i)) return 0;
    for (size_t i=0;i<p->available_count;i++) if (!mulib_cdd_name_valid(p->available[i].name) || !p->available[i].revision) return 0;
    return 1;
}
int mulib_cdd_satisfied(const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p,uint64_t selected,const mulib_cdd_req_t *r) {
    if (!c || c->count>MULIB_CDD_MAX_ENTRIES || (!c->entries && c->count) ||
        !p || (!p->available && p->available_count) || !r) return 0;
    for (size_t i=0;i<p->available_count;i++) if (mulib_cdd_cap_matches(p->available+i,r)) return 1;
    for (size_t i=0;i<c->count;i++) if ((selected & (UINT64_C(1)<<i)) && mulib_cdd_entry_provides(c->entries+i,r)) return 1;
    return 0;
}
int mulib_cdd_can_append(const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p,uint64_t selected,size_t i) {
    if (!c || !c->entries || i>=c->count || c->count>MULIB_CDD_MAX_ENTRIES ||
        !p || (!p->available && p->available_count)) return 0;
    uint64_t bit=UINT64_C(1)<<i;
    if (!(p->allowed & bit) || (selected & bit)) return 0;
    const mulib_cdd_entry_t *e=c->entries+i;
    if (!e->requirements && e->require_count) return 0;
    for (size_t j=0;j<c->count;j++) if ((selected & (UINT64_C(1)<<j)) && mulib_cdd_entries_conflict(e,c->entries+j)) return 0;
    for (size_t j=0;j<e->require_count;j++) if (!mulib_cdd_satisfied(c,p,selected,e->requirements+j)) return 0;
    return 1;
}
mulib_cdd_status_t mulib_cdd_plan_validate(const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p,const mulib_cdd_plan_t *plan,size_t *failed) {
    if (failed) *failed=MULIB_CDD_NONE;
    mulib_cdd_status_t s=mulib_cdd_validate(c,NULL); if (s) return s;
    if (!mulib_cdd_problem_valid(p) || !plan || plan->found!=1 || plan->count>c->count) return MULIB_CDD_INVALID;
    uint64_t selected=0,cost=0;
    for (size_t k=0;k<plan->count;k++) {
        size_t i=plan->order[k];
        if (failed) *failed=k;
        if (i>=c->count || !(p->allowed & (UINT64_C(1)<<i))) return MULIB_CDD_INVALID;
        if (selected & (UINT64_C(1)<<i)) return MULIB_CDD_DUPLICATE;
        const mulib_cdd_entry_t *e=c->entries+i;
        for (size_t j=0;j<c->count;j++) if ((selected & (UINT64_C(1)<<j)) && mulib_cdd_entries_conflict(e,c->entries+j)) return MULIB_CDD_CONFLICT;
        for (size_t j=0;j<e->require_count;j++) if (!mulib_cdd_satisfied(c,p,selected,e->requirements+j)) return MULIB_CDD_UNSAT;
        selected|=UINT64_C(1)<<i; cost+=e->cost;
        if (cost>p->max_cost) return MULIB_CDD_UNSAT;
    }
    if (failed) *failed=plan->count;
    if (cost!=plan->cost) return MULIB_CDD_INVALID;
    for (size_t j=0;j<p->goal_count;j++) if (!mulib_cdd_satisfied(c,p,selected,p->goals+j)) return MULIB_CDD_UNSAT;
    return MULIB_CDD_OK;
}
#ifdef __cplusplus
}
#endif
#endif
