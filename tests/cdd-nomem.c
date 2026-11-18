/* The standalone runner links this with --wrap=calloc for fault injection. */
#include "mulib-cdd-resolve.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>

static size_t allocation_attempts;

void *__wrap_calloc(size_t count,size_t size);
void *__wrap_calloc(size_t count,size_t size) {
    assert(count && size);
    allocation_attempts++;
    return NULL;
}

int main(void) {
    const mulib_cdd_cap_t cap={"ready",1};
    const mulib_cdd_entry_t entry={"ready","test","label",1,4,&cap,1,NULL,0,NULL,0,NULL,0};
    const mulib_cdd_catalogue_t c={&entry,1};
    const mulib_cdd_req_t goal={"ready",1,0};
    const mulib_cdd_problem_t p={&goal,1,NULL,0,UINT64_MAX,UINT64_MAX};
    mulib_cdd_plan_t plan={{63},1,4,1};
    mulib_cdd_resolve_stats_t stats={1,1,1};
    assert(mulib_cdd_resolve(&c,&p,32,&plan,&stats)==MULIB_CDD_NOMEM);
    assert(allocation_attempts==1);
    assert(!plan.found && !plan.count && !plan.cost && !plan.order[0]);
    assert(!stats.expanded && !stats.memo_hits && !stats.optimal);
    puts("CDD allocation failure test passed");
    return 0;
}
