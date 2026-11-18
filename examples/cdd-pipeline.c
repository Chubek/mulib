/* Self-contained example: select a compiler pipeline and dispatch its steps. */
#define MULIB_IMPL_CDD_CATALOGUE
#define MULIB_IMPL_CDD_QUERY
#define MULIB_IMPL_CDD_PLAN
#define MULIB_IMPL_CDD_RESOLVE
#define MULIB_IMPL_CDD_EXECUTE
#include "mulib-cdd-query.h"
#include "mulib-cdd-resolve.h"
#include "mulib-cdd-execute.h"
#include <stdio.h>
#include <string.h>
static const mulib_cdd_cap_t ssa[]={{"ir.ssa",1}}, live[]={{"analysis.live",1}}, allocated[]={{"ir.allocated",1}};
static const mulib_cdd_req_t need_ssa[]={{"ir.ssa",1,1}}, need_live[]={{"analysis.live",1,1}}, goal[]={{"ir.allocated",1,1}};
static const mulib_cdd_attr_t jit[]={{"jit",{MULIB_CDD_BOOL,1,NULL}}};
static const mulib_cdd_entry_t components[]={
    {"lower","frontend","lower_ssa",1,3,ssa,1,NULL,0,NULL,0,NULL,0},
    {"liveness","analysis","run_liveness",1,2,live,1,need_ssa,1,NULL,0,NULL,0},
    {"linear-scan","allocator","allocate_linear",1,4,allocated,1,need_live,1,NULL,0,jit,1},
    {"graph-coloring","allocator","allocate_graph",1,8,allocated,1,need_live,1,NULL,0,NULL,0}
};
struct pipeline { size_t phase; };
static int run(const mulib_cdd_entry_t *e,void *ctx) {
    struct pipeline *pipeline=(struct pipeline *)ctx;
    /* This host approves only its first three immutable descriptors, in order.
     * The implementation label is for display, not a symbol to resolve. */
    if (pipeline->phase>=3 || e!=components+pipeline->phase) return 1;
    pipeline->phase++;
    printf("execute %s (%s)\n",e->id,e->implementation); return 0;
}
int main(void) {
    mulib_cdd_catalogue_t c={components,sizeof components/sizeof *components};
    mulib_cdd_pred_t predicate={"jit",MULIB_CDD_EQ,{MULIB_CDD_BOOL,1,NULL}};
    mulib_cdd_query_t q={"allocator",goal,1,&predicate,1};
    size_t ids[MULIB_CDD_MAX_ENTRIES],n;
    if(mulib_cdd_query_run(&c,&q,ids,MULIB_CDD_MAX_ENTRIES,&n) || !n) return 1;
    /* Allow all non-allocators, but only allocators matching the JIT policy. */
    uint64_t allowed=0;
    for(size_t i=0;i<c.count;i++) if(strcmp(c.entries[i].category,"allocator")) allowed|=UINT64_C(1)<<i;
    for(size_t i=0;i<n;i++) allowed|=UINT64_C(1)<<ids[i];
    mulib_cdd_problem_t p={goal,1,NULL,0,allowed,20};
    mulib_cdd_plan_t plan; mulib_cdd_resolve_stats_t stats;
    if(mulib_cdd_resolve(&c,&p,1000,&plan,&stats)!=MULIB_CDD_OK) return 2;
    printf("selected %zu entries, cost %llu\n",plan.count,(unsigned long long)plan.cost);
    struct pipeline pipeline={0};
    mulib_cdd_executor_t x={run,NULL,&pipeline}; mulib_cdd_exec_report_t report;
    return mulib_cdd_execute(&c,&p,&plan,&x,&report)!=MULIB_CDD_OK;
}
