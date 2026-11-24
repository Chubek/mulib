/** @file mulib-cdd-resolve.h
 * @brief Exact bounded capability composition with branch-and-bound and subset memoization.
 * ISO C11; define MULIB_IMPL_CDD_RESOLVE in exactly one translation unit.
 * See manual/mulib-cdd-resolve.md for ownership and error contracts.
 */
#ifndef MULIB_CDD_RESOLVE_H
#define MULIB_CDD_RESOLVE_H
#include <stddef.h>
#include <stdint.h>
#include "mulib-cdd-plan.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { size_t expanded, memo_hits; int optimal; } mulib_cdd_resolve_stats_t;
/* OK: proven minimum cost, then minimum count. UNSAT: proven impossible.
 * LIMIT: search stopped, plan.found indicates whether feasible incumbent exists.
 * max_nodes counts expanded states; zero and oversized catalogues are invalid. */
mulib_cdd_status_t mulib_cdd_resolve(const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p,size_t max_nodes,mulib_cdd_plan_t *out,mulib_cdd_resolve_stats_t *stats);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_CDD_RESOLVE_H */
#if defined(MULIB_IMPL_CDD_RESOLVE) && !defined(MULIB_CDD_RESOLVE_IMPLEMENTED)
#define MULIB_CDD_RESOLVE_IMPLEMENTED
#include <stdlib.h>
#include <string.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint64_t key; unsigned char used; } mulib_cdd_memo_slot_;
typedef struct {
    const mulib_cdd_catalogue_t *c; const mulib_cdd_problem_t *p;
    size_t budget, table_size, table_count;
    mulib_cdd_memo_slot_ *table;
    mulib_cdd_plan_t current,best;
    mulib_cdd_resolve_stats_t stats;
    uint64_t relevant, providers[MULIB_CDD_MAX_ENTRIES];
    int limited;
} mulib_cdd_search_;
static int mulib_cdd_memo_seen_(mulib_cdd_search_ *s,uint64_t key) {
    uint64_t h=key;
    h^=h>>30; h*=UINT64_C(0xbf58476d1ce4e5b9); h^=h>>27; h*=UINT64_C(0x94d049bb133111eb); h^=h>>31;
    size_t pos=(size_t)h & (s->table_size-1);
    for (size_t k=0;k<s->table_size;k++) {
        mulib_cdd_memo_slot_ *e=s->table+pos;
        if (!e->used) {
            if (s->table_count<s->table_size/2) { e->key=key; e->used=1; s->table_count++; }
            return 0;
        }
        if (e->key==key) { s->stats.memo_hits++; return 1; }
        pos=(pos+1)&(s->table_size-1);
    }
    return 0;
}
/* Transitive over-approximation of all providers relevant to the goals. */
static uint64_t mulib_cdd_relevant_(mulib_cdd_search_ *s) {
    uint64_t relevant=0,old;
    for (size_t i=0;i<s->c->count;i++) {
        if (!(s->p->allowed & (UINT64_C(1)<<i))) continue;
        const mulib_cdd_entry_t *e=s->c->entries+i;
        for (size_t j=0;j<s->p->goal_count;j++) if (mulib_cdd_entry_provides(e,s->p->goals+j)) relevant|=UINT64_C(1)<<i;
        for (size_t k=0;k<s->c->count;k++) {
            const mulib_cdd_entry_t *consumer=s->c->entries+k;
            for (size_t j=0;j<consumer->require_count;j++) if (mulib_cdd_entry_provides(e,consumer->requirements+j)) s->providers[k]|=UINT64_C(1)<<i;
        }
    }
    do {
        old=relevant;
        for (size_t i=0;i<s->c->count;i++) if (relevant & (UINT64_C(1)<<i)) relevant|=s->providers[i];
    } while (old!=relevant);
    return relevant;
}
static int mulib_cdd_goals_(mulib_cdd_search_ *s,uint64_t selected) {
    for (size_t j=0;j<s->p->goal_count;j++) if (!mulib_cdd_satisfied(s->c,s->p,selected,s->p->goals+j)) return 0;
    return 1;
}
static void mulib_cdd_search_visit_(mulib_cdd_search_ *s,uint64_t selected) {
    if (s->limited || s->current.cost>s->p->max_cost) return;
    if (s->best.found && (s->current.cost>s->best.cost || (s->current.cost==s->best.cost && s->current.count>=s->best.count))) return;
    if (mulib_cdd_memo_seen_(s,selected)) return;
    if (s->stats.expanded==s->budget) { s->limited=1; return; }
    s->stats.expanded++;
    if (mulib_cdd_goals_(s,selected)) { s->best=s->current; s->best.found=1; return; }
    /* Cheapest candidates first; ties by id. All feasible branches still explored. */
    size_t candidates[MULIB_CDD_MAX_ENTRIES],n=0;
    for (size_t i=0;i<s->c->count;i++) {
        if (!(s->relevant & (UINT64_C(1)<<i)) || !mulib_cdd_can_append(s->c,s->p,selected,i)) continue;
        const mulib_cdd_entry_t *e=s->c->entries+i;
        if (e->cost>s->p->max_cost-s->current.cost) continue;
        size_t j=n;
        while (j) {
            const mulib_cdd_entry_t *a=s->c->entries+candidates[j-1];
            if (a->cost<e->cost || (a->cost==e->cost && strcmp(a->id,e->id)<0)) break;
            candidates[j]=candidates[j-1]; j--;
        }
        candidates[j]=i; n++;
    }
    for (size_t k=0;k<n && !s->limited;k++) {
        size_t i=candidates[k]; uint32_t cost=s->c->entries[i].cost;
        s->current.order[s->current.count++]=i; s->current.cost+=cost;
        mulib_cdd_search_visit_(s,selected | (UINT64_C(1)<<i));
        s->current.count--; s->current.cost-=cost;
    }
}
mulib_cdd_status_t mulib_cdd_resolve(const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p,size_t max_nodes,mulib_cdd_plan_t *out,mulib_cdd_resolve_stats_t *stats) {
    if (stats) memset(stats,0,sizeof(*stats));
    if (!out) return MULIB_CDD_INVALID;
    memset(out,0,sizeof(*out));
    /* The catalogue validator uses LIMIT for its size ceiling. For this API
     * reserve LIMIT for a search actually stopped by max_nodes. */
    mulib_cdd_status_t status=mulib_cdd_validate(c,NULL);
    if (status) return status==MULIB_CDD_LIMIT?MULIB_CDD_INVALID:status;
    if (!max_nodes || !mulib_cdd_problem_valid(p)) return MULIB_CDD_INVALID;
    mulib_cdd_search_ s; memset(&s,0,sizeof(s)); s.c=c; s.p=p; s.budget=max_nodes;
    s.table_size=16;
    while (s.table_size/2<max_nodes && s.table_size<1048576u) s.table_size*=2;
    s.table=(mulib_cdd_memo_slot_*)calloc(s.table_size,sizeof(*s.table));
    if (!s.table) return MULIB_CDD_NOMEM;
    s.relevant=mulib_cdd_relevant_(&s);
    mulib_cdd_search_visit_(&s,0); free(s.table);
    *out=s.best; s.stats.optimal=!s.limited && s.best.found;
    if (stats) *stats=s.stats;
    return s.limited?MULIB_CDD_LIMIT: (out->found?MULIB_CDD_OK:MULIB_CDD_UNSAT);
}
#ifdef __cplusplus
}
#endif
#endif
