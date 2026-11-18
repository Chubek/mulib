#include "mulib-cdd-registry.h"
#include "mulib-cdd-query.h"
#include "mulib-cdd-resolve.h"
#include "mulib-cdd-schema.h"
#include "mulib-cdd-execute.h"
#include "mulib-cdd-diff.h"
/* This suite uses assertions to run the tests even in release builds. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const mulib_cdd_cap_t source[]={{"source",1}}, ir[]={{"ir",2}}, output[]={{"output",1}}, cycle[]={{"cycle",1}};
static const mulib_cdd_req_t need_source[]={{"source",1,1}}, need_ir[]={{"ir",2,2}}, need_cycle[]={{"cycle",1,0}}, need_output[]={{"output",1,0}};
static const char *const bad_conflict[]={"emit"};
static const mulib_cdd_attr_t attrs[]={{"quality",{MULIB_CDD_INT,90,NULL}},{"jit",{MULIB_CDD_BOOL,1,NULL}},{"mode",{MULIB_CDD_STRING,0,"native"}}};
static const mulib_cdd_entry_t entries[]={
    {"source","input","source_fn",1,1,source,1,NULL,0,NULL,0,NULL,0},
    {"bad","analysis","bad_fn",1,1,ir,1,need_source,1,bad_conflict,1,NULL,0},
    {"good","analysis","good_fn",1,4,ir,1,need_source,1,NULL,0,attrs,3},
    {"emit","output","emit_fn",1,2,output,1,need_ir,1,NULL,0,NULL,0},
    {"direct","output","direct_fn",1,12,output,1,NULL,0,NULL,0,NULL,0},
    {"cycle","analysis","cycle_fn",1,0,cycle,1,need_cycle,1,NULL,0,NULL,0}
};
static const mulib_cdd_catalogue_t catalogue={entries,6};
static const mulib_cdd_problem_t problem={need_output,1,NULL,0,UINT64_MAX,UINT64_MAX};
static void test_catalogue(void) {
    mulib_cdd_diag_t d;
    assert(!mulib_cdd_validate(&catalogue,&d) && d.status==0);
    assert(mulib_cdd_find(&catalogue,"good")==2);
    assert(mulib_cdd_find(&catalogue,"unknown")==MULIB_CDD_NONE);
    assert(mulib_cdd_entries_conflict(entries+3,entries+1));
    assert(!mulib_cdd_entry_provides(entries,need_ir));
    mulib_cdd_entry_t bad[2]={entries[0],entries[0]};
    mulib_cdd_catalogue_t c={bad,2};
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_DUPLICATE && d.entry==1);
    bad[1]=entries[1]; bad[1].provides=NULL;
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_INVALID && d.entry==1);
    bad[1]=entries[1]; bad[1].id="bad id";
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_INVALID);
    mulib_cdd_registry_t r; mulib_cdd_registry_init(&r);
    assert(!mulib_cdd_registry_add(&r,entries));
    assert(mulib_cdd_registry_add(&r,entries)==MULIB_CDD_DUPLICATE && r.count==1);
    assert(!mulib_cdd_registry_add(&r,entries+1));
    assert(!mulib_cdd_registry_remove(&r,"source"));
    c=mulib_cdd_registry_view(&r); assert(c.count==1 && mulib_cdd_find(&c,"bad")==0);
    assert(mulib_cdd_registry_remove(&r,"source")==MULIB_CDD_NOT_FOUND);
}
static void test_catalogue_failures(void) {
    mulib_cdd_diag_t d;
    mulib_cdd_catalogue_t c={NULL,1};
    assert(mulib_cdd_validate(NULL,&d)==MULIB_CDD_INVALID);
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_INVALID && d.entry==MULIB_CDD_NONE);
    assert(mulib_cdd_find(NULL,"source")==MULIB_CDD_NONE);
    assert(mulib_cdd_find(&c,"source")==MULIB_CDD_NONE);
    assert(!mulib_cdd_name_valid(NULL) && !mulib_cdd_name_valid(""));
    assert(!mulib_cdd_value_valid(NULL) && !mulib_cdd_req_valid(NULL));
    assert(!mulib_cdd_cap_matches(NULL,need_source) && !mulib_cdd_cap_matches(source,NULL));
    assert(!mulib_cdd_entry_provides(NULL,need_source) && !mulib_cdd_entries_conflict(NULL,entries));
    assert(!mulib_cdd_attr_get(NULL,"mode") && !mulib_cdd_attr_get(entries+2,NULL));
    assert(mulib_cdd_attr_get(entries+2,"mode")==attrs+2);

    mulib_cdd_entry_t e=entries[1];
    c.entries=&e;
    mulib_cdd_cap_t caps[]={ir[0],ir[0]};
    e.provides=caps; e.provide_count=2;
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_DUPLICATE && d.entry==0 && d.item==1 && !strcmp(d.field,"provides"));
    caps[1].revision=0;
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_INVALID && d.item==1);
    caps[1].revision=3;
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_OK && d.entry==MULIB_CDD_NONE && d.item==MULIB_CDD_NONE && !d.field);
    mulib_cdd_req_t req={"source",2,1};
    e.requirements=&req;
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_INVALID && d.item==0 && !strcmp(d.field,"requirements"));
    e=entries[1];
    const char *conflicts[]={"emit","bad"};
    e.conflicts=conflicts; e.conflict_count=2;
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_INVALID && d.item==1 && !strcmp(d.field,"conflicts"));
    conflicts[1]="emit";
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_DUPLICATE && d.item==1);
    e=entries[2];
    mulib_cdd_attr_t values[]={attrs[1],attrs[1]};
    e.attrs=values; e.attr_count=2;
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_DUPLICATE && d.item==1 && !strcmp(d.field,"attrs"));
    values[1].value.integer=2;
    assert(mulib_cdd_validate(&c,&d)==MULIB_CDD_INVALID && d.item==1);
}
static void test_registry_borrowing(void) {
    mulib_cdd_registry_t reg;
    mulib_cdd_registry_init(NULL);
    mulib_cdd_catalogue_t view=mulib_cdd_registry_view(NULL);
    assert(!view.entries && !view.count && mulib_cdd_validate(&view,NULL)==MULIB_CDD_OK);
    mulib_cdd_registry_init(&reg);
    assert(mulib_cdd_registry_add(NULL,entries)==MULIB_CDD_INVALID);
    assert(mulib_cdd_registry_add(&reg,NULL)==MULIB_CDD_INVALID);
    assert(mulib_cdd_registry_remove(NULL,"good")==MULIB_CDD_INVALID);
    assert(mulib_cdd_registry_remove(&reg,NULL)==MULIB_CDD_INVALID);
    /* The descriptor record itself is copied, its pointed-to data is not. */
    mulib_cdd_entry_t local=entries[2];
    assert(mulib_cdd_registry_add(&reg,&local)==MULIB_CDD_OK);
    view=mulib_cdd_registry_view(&reg);
    assert(view.entries!=&local && view.entries[0].attrs==local.attrs);
    assert(view.entries[0].provides==local.provides && view.entries[0].requirements==local.requirements);
    local.id=NULL;
    assert(mulib_cdd_registry_add(&reg,&local)==MULIB_CDD_INVALID);
    view=mulib_cdd_registry_view(&reg);
    assert(view.count==1 && mulib_cdd_find(&view,"good")==0);
    assert(mulib_cdd_registry_remove(&reg,"good")==MULIB_CDD_OK);
    view=mulib_cdd_registry_view(&reg);
    assert(!view.count);
}
static void test_query_schema(void) {
    size_t needed, indices[6];
    mulib_cdd_query_t q={"analysis",need_ir,1,NULL,0};
    assert(mulib_cdd_query_run(&catalogue,&q,indices,1,&needed)==MULIB_CDD_LIMIT && needed==2 && indices[0]==1);
    assert(!mulib_cdd_query_run(&catalogue,&q,indices,6,&needed) && indices[1]==2);
    mulib_cdd_pred_t preds[]={{"quality",MULIB_CDD_GE,{MULIB_CDD_INT,80,NULL}},{"jit",MULIB_CDD_EQ,{MULIB_CDD_BOOL,1,NULL}},{"mode",MULIB_CDD_EQ,{MULIB_CDD_STRING,0,"native"}}};
    q.preds=preds; q.pred_count=3;
    assert(!mulib_cdd_query_run(&catalogue,&q,indices,6,&needed) && needed==1 && indices[0]==2);
    preds[0].op=MULIB_CDD_LT;
    assert(!mulib_cdd_query_run(&catalogue,&q,indices,6,&needed) && !needed);
    preds[0].op=MULIB_CDD_EXISTS; preds[0].value.kind=(mulib_cdd_kind_t)0;
    assert(!mulib_cdd_query_run(&catalogue,&q,indices,6,&needed) && needed==1);
    mulib_cdd_field_t f[]={ {"quality",MULIB_CDD_INT,1,1,0,100},{"jit",MULIB_CDD_BOOL,1,0,0,0},{"mode",MULIB_CDD_STRING,1,0,0,0} };
    mulib_cdd_schema_t s={"analysis",f,3,1}; mulib_cdd_diag_t d;
    assert(mulib_cdd_schema_validate(&catalogue,&s,&d)==MULIB_CDD_NOT_FOUND && d.entry==1);
    mulib_cdd_catalogue_t c={entries+2,1};
    assert(!mulib_cdd_schema_validate(&c,&s,&d));
    f[0].max=89; assert(mulib_cdd_schema_validate(&c,&s,&d)==MULIB_CDD_INVALID);
    f[0].max=100; s.field_count=2; assert(mulib_cdd_schema_validate(&c,&s,&d)==MULIB_CDD_INVALID);
    s.closed=0; assert(!mulib_cdd_schema_validate(&c,&s,&d));
}
static void test_query_schema_failures(void) {
    const mulib_cdd_query_t all={NULL,NULL,0,NULL,0};
    mulib_cdd_query_t q=all;
    mulib_cdd_entry_t tied[]={entries[2],entries[1]};
    tied[0].cost=tied[1].cost;
    mulib_cdd_catalogue_t c={tied,2};
    size_t out[]={MULIB_CDD_NONE,MULIB_CDD_NONE},n=9;
    assert(mulib_cdd_query_run(&c,&q,out,1,&n)==MULIB_CDD_LIMIT);
    assert(n==2 && out[0]==1 && out[1]==MULIB_CDD_NONE); /* tie by ID, not catalogue order */
    assert(mulib_cdd_query_run(&c,&q,NULL,0,&n)==MULIB_CDD_LIMIT && n==2);
    assert(mulib_cdd_query_run(&c,&q,NULL,1,&n)==MULIB_CDD_INVALID && n==0);
    assert(mulib_cdd_query_run(&c,&q,out,2,NULL)==MULIB_CDD_INVALID && out[0]==1 && out[1]==MULIB_CDD_NONE);
    assert(!mulib_cdd_query_valid(NULL) && !mulib_cdd_query_matches(NULL,&q));
    const mulib_cdd_req_t bad={"ir",0,0};
    q.caps=&bad; q.cap_count=1; n=9;
    assert(mulib_cdd_query_run(&c,&q,out,2,&n)==MULIB_CDD_INVALID && n==0);
    assert(out[0]==1 && out[1]==MULIB_CDD_NONE);
    mulib_cdd_pred_t pred={"mode",MULIB_CDD_LT,{MULIB_CDD_STRING,0,"native"}};
    q=all; q.preds=&pred; q.pred_count=1;
    assert(!mulib_cdd_query_valid(&q) && !mulib_cdd_query_matches(entries+2,&q));
    pred.op=MULIB_CDD_NE;
    assert(mulib_cdd_query_valid(&q) && !mulib_cdd_query_matches(entries,&q)); /* missing != anything is false */
    pred.value.kind=MULIB_CDD_INT;
    assert(!mulib_cdd_query_matches(entries+2,&q)); /* unlike types never match */
    assert(mulib_cdd_query_run(&c,&q,NULL,0,&n)==MULIB_CDD_OK && n==0);

    mulib_cdd_schema_t s={NULL,NULL,1,0};
    mulib_cdd_diag_t d;
    assert(mulib_cdd_schema_validate(&c,&s,&d)==MULIB_CDD_INVALID && d.entry==MULIB_CDD_NONE);
    mulib_cdd_field_t fields[]={{"mode",MULIB_CDD_STRING,0,0,0,0},{"mode",MULIB_CDD_STRING,0,0,0,0}};
    s.fields=fields; s.field_count=2;
    assert(mulib_cdd_schema_validate(&c,&s,&d)==MULIB_CDD_DUPLICATE && d.entry==MULIB_CDD_NONE && d.item==1);
    s.field_count=1; fields[0].bounded=1;
    assert(mulib_cdd_schema_validate(&c,&s,&d)==MULIB_CDD_INVALID && d.item==0);
    fields[0].bounded=0; fields[0].kind=MULIB_CDD_BOOL;
    assert(mulib_cdd_schema_validate(&c,&s,&d)==MULIB_CDD_INVALID && d.entry==0 && d.item==0);
    assert(d.field==fields[0].key); /* mismatch borrows the actual field key */

    tied[1].id=tied[0].id; n=9;
    assert(mulib_cdd_query_run(&c,&all,out,2,&n)==MULIB_CDD_DUPLICATE && n==0);
    assert(out[0]==1 && out[1]==MULIB_CDD_NONE);
    assert(mulib_cdd_schema_validate(&c,&s,&d)==MULIB_CDD_DUPLICATE && d.entry==1);
}
static void test_resolver(void) {
    mulib_cdd_plan_t plan; mulib_cdd_resolve_stats_t st; size_t fail;
    assert(!mulib_cdd_resolve(&catalogue,&problem,10000,&plan,&st));
    assert(st.optimal && plan.found && plan.cost==7 && plan.count==3);
    assert(plan.order[0]==0 && plan.order[1]==2 && plan.order[2]==3);
    assert(!mulib_cdd_plan_validate(&catalogue,&problem,&plan,&fail));
    size_t tmp=plan.order[0]; plan.order[0]=plan.order[1]; plan.order[1]=tmp;
    assert(mulib_cdd_plan_validate(&catalogue,&problem,&plan,&fail)==MULIB_CDD_UNSAT && fail==0);
    mulib_cdd_problem_t p=problem; p.max_cost=6;
    assert(mulib_cdd_resolve(&catalogue,&p,10000,&plan,&st)==MULIB_CDD_UNSAT && !plan.found);
    p=problem; p.available=ir; p.available_count=1;
    assert(!mulib_cdd_resolve(&catalogue,&p,10000,&plan,&st) && plan.cost==2 && plan.count==1);
    p=problem; p.goals=need_cycle; p.goal_count=1;
    assert(mulib_cdd_resolve(&catalogue,&p,100,&plan,&st)==MULIB_CDD_UNSAT);
    p.available=cycle; p.available_count=1;
    assert(!mulib_cdd_resolve(&catalogue,&p,1,&plan,&st) && plan.count==0);
    assert(mulib_cdd_resolve(&catalogue,&problem,1,&plan,&st)==MULIB_CDD_LIMIT && !plan.found && !st.optimal);
    int found_limited=0;
    for(size_t k=2;k<40;k++) {
        mulib_cdd_status_t rc=mulib_cdd_resolve(&catalogue,&problem,k,&plan,&st);
        if(rc==MULIB_CDD_LIMIT && plan.found) { assert(!st.optimal && !mulib_cdd_plan_validate(&catalogue,&problem,&plan,NULL)); found_limited=1; }
    }
    assert(found_limited);
    p=problem; p.allowed=UINT64_C(1)<<4;
    assert(!mulib_cdd_resolve(&catalogue,&p,100,&plan,&st) && plan.cost==12);
    p.allowed=0; assert(mulib_cdd_resolve(&catalogue,&p,100,&plan,&st)==MULIB_CDD_UNSAT);
}
static void test_budget(void) {
    mulib_cdd_entry_t e[]={entries[0],entries[4],entries[3]};
    e[1].cost=5; e[2].cost=8; e[2].requirements=need_source;
    mulib_cdd_catalogue_t c={e,3};
    mulib_cdd_plan_t plan;
    mulib_cdd_resolve_stats_t st;
    assert(mulib_cdd_resolve(&c,&problem,2,&plan,&st)==MULIB_CDD_LIMIT);
    assert(st.expanded==2 && !st.optimal && !plan.found && !plan.count && !plan.cost);
    assert(mulib_cdd_resolve(&c,&problem,3,&plan,&st)==MULIB_CDD_LIMIT);
    assert(st.expanded==3 && !st.optimal && plan.found && plan.count==2 && plan.cost==6);
    assert(mulib_cdd_plan_validate(&c,&problem,&plan,NULL)==MULIB_CDD_OK);
    assert(mulib_cdd_resolve(&c,&problem,4,&plan,&st)==MULIB_CDD_OK);
    assert(st.expanded==4 && st.optimal && plan.found && plan.count==1 && plan.cost==5 && plan.order[0]==1);
    assert(mulib_cdd_resolve(&c,&problem,0,&plan,&st)==MULIB_CDD_INVALID);
    assert(!st.expanded && !st.memo_hits && !st.optimal && !plan.found && !plan.count && !plan.cost);
    assert(mulib_cdd_resolve(&c,&problem,3,NULL,&st)==MULIB_CDD_INVALID);
    assert(!st.expanded && !st.memo_hits && !st.optimal);

    /* Four distinct reachable subsets: the second order reaching {source, good}
     * is not an extra expansion. Emit requires a capability absent everywhere. */
    const mulib_cdd_req_t prereqs[]={{"source",1,1},{"ir",2,2},{"missing",1,0}};
    e[1]=entries[2]; e[1].require_count=0;
    e[2].requirements=prereqs; e[2].require_count=3;
    assert(mulib_cdd_resolve(&c,&problem,3,&plan,&st)==MULIB_CDD_LIMIT);
    assert(st.expanded==3 && !st.optimal && !plan.found);
    assert(mulib_cdd_resolve(&c,&problem,4,&plan,&st)==MULIB_CDD_UNSAT);
    assert(st.expanded==4 && st.memo_hits==1 && !st.optimal && !plan.found);
}
static void test_plan_failures(void) {
    mulib_cdd_problem_t p=problem;
    mulib_cdd_plan_t plan={{0,2,3},3,7,1};
    size_t failed;
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_OK && failed==3);
    assert(!mulib_cdd_problem_valid(NULL));
    p.goals=NULL;
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_INVALID && failed==MULIB_CDD_NONE);
    mulib_cdd_plan_t rejected=plan;
    mulib_cdd_resolve_stats_t stats={1,1,1};
    assert(mulib_cdd_resolve(&catalogue,&p,100,&rejected,&stats)==MULIB_CDD_INVALID);
    assert(!rejected.found && !rejected.count && !rejected.cost && !rejected.order[1]);
    assert(!stats.expanded && !stats.memo_hits && !stats.optimal);
    p=problem; p.available_count=1;
    assert(!mulib_cdd_problem_valid(&p));
    assert(!mulib_cdd_satisfied(&catalogue,&p,0,need_output) && !mulib_cdd_can_append(&catalogue,&p,0,0));
    p=problem;
    mulib_cdd_catalogue_t bad={NULL,1};
    assert(!mulib_cdd_satisfied(&bad,&p,1,need_output) && !mulib_cdd_can_append(&bad,&p,0,0));
    assert(!mulib_cdd_satisfied(NULL,&p,0,need_output) && !mulib_cdd_satisfied(&catalogue,NULL,0,need_output));
    mulib_cdd_entry_t malformed=entries[1];
    malformed.requirements=NULL; bad.entries=&malformed;
    assert(!mulib_cdd_can_append(&bad,&p,0,0));
    assert(mulib_cdd_plan_validate(&catalogue,&p,NULL,&failed)==MULIB_CDD_INVALID && failed==MULIB_CDD_NONE);
    plan.found=0;
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_INVALID && failed==MULIB_CDD_NONE);
    plan.found=1; plan.count=MULIB_CDD_MAX_ENTRIES+1;
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_INVALID && failed==MULIB_CDD_NONE);
    plan.count=3; plan.order[1]=MULIB_CDD_MAX_ENTRIES;
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_INVALID && failed==1);
    plan.order[1]=0;
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_DUPLICATE && failed==1);
    plan.order[1]=1;
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_CONFLICT && failed==2);
    plan.order[1]=2; p.allowed &= ~(UINT64_C(1)<<2);
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_INVALID && failed==1);
    p=problem; p.max_cost=6;
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_UNSAT && failed==2);
    p=problem; plan.count=2; plan.cost=5;
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_UNSAT && failed==2);
    plan.cost=4;
    assert(mulib_cdd_plan_validate(&catalogue,&p,&plan,&failed)==MULIB_CDD_INVALID && failed==2);
}
struct execution { char history[32]; size_t n; int fail,undo_fail; };
static int apply(const mulib_cdd_entry_t *e,void *ctx) {
    struct execution *x=(struct execution *)ctx;
    if (e!=entries && e!=entries+2 && e!=entries+3) return 23; /* host policy */
    assert(x->n<sizeof x->history-1);
    x->history[x->n++]=e->id[0];
    return x->fail && e==entries+3?17:0;
}
static int undo(const mulib_cdd_entry_t *e,void *ctx) {
    struct execution *x=(struct execution *)ctx;
    assert(e==entries || e==entries+2 || e==entries+3);
    assert(x->n<sizeof x->history-1);
    x->history[x->n++]=(char)(e->id[0]-'a'+'A');
    return x->undo_fail && e==entries+2;
}
static void test_exec_diff(void) {
    mulib_cdd_plan_t plan; assert(!mulib_cdd_resolve(&catalogue,&problem,1000,&plan,NULL));
    struct execution ex={{0},0,1,1}; mulib_cdd_executor_t x={apply,undo,&ex}; mulib_cdd_exec_report_t r;
    assert(mulib_cdd_execute(&catalogue,&problem,&plan,&x,&r)==MULIB_CDD_CALLBACK);
    assert(r.completed==2 && r.failed_step==2 && r.callback_code==17 && r.undo_attempted==2 && r.undo_failed==1);
    assert(!strcmp(ex.history,"sgeGS"));
    x.undo=NULL; memset(&ex,0,sizeof ex); ex.fail=1;
    assert(mulib_cdd_execute(&catalogue,&problem,&plan,&x,&r)==MULIB_CDD_CALLBACK);
    assert(!strcmp(ex.history,"sge") && r.completed==2 && r.failed_step==2 && r.callback_code==17 && !r.undo_attempted && !r.undo_failed);
    x.undo=undo;
    memset(&ex,0,sizeof ex); assert(!mulib_cdd_execute(&catalogue,&problem,&plan,&x,&r) && !strcmp(ex.history,"sge"));
    assert(r.completed==3 && r.failed_step==MULIB_CDD_NONE && !r.callback_code && !r.undo_attempted && !r.undo_failed);
    plan.cost++; memset(&ex,0,sizeof ex);
    assert(mulib_cdd_execute(&catalogue,&problem,&plan,&x,&r)==MULIB_CDD_INVALID && !ex.n);
    assert(!r.completed && r.failed_step==3 && !r.callback_code && !r.undo_attempted && !r.undo_failed);
    plan.cost--;
    assert(mulib_cdd_execute(&catalogue,&problem,&plan,&x,NULL)==MULIB_CDD_INVALID && !ex.n);
    assert(mulib_cdd_execute(&catalogue,&problem,&plan,NULL,&r)==MULIB_CDD_INVALID);
    assert(!r.completed && r.failed_step==MULIB_CDD_NONE && !r.callback_code && !r.undo_attempted && !r.undo_failed);
    mulib_cdd_executor_t missing={NULL,undo,&ex};
    assert(mulib_cdd_execute(&catalogue,&problem,&plan,&missing,&r)==MULIB_CDD_INVALID && !ex.n);
    /* This valid plan names a component the host has not authorized. */
    mulib_cdd_plan_t unapproved={{4},1,12,1};
    assert(mulib_cdd_plan_validate(&catalogue,&problem,&unapproved,NULL)==MULIB_CDD_OK);
    assert(mulib_cdd_execute(&catalogue,&problem,&unapproved,&x,&r)==MULIB_CDD_CALLBACK);
    assert(!ex.n && !r.completed && r.failed_step==0 && r.callback_code==23 && !r.undo_attempted && !r.undo_failed);

    mulib_cdd_entry_t changed[]={entries[2],entries[0],entries[4]}; changed[0].cost++;
    changed[2].id="new"; mulib_cdd_catalogue_t c={changed,3}; mulib_cdd_change_t out[8]; size_t n;
    assert(mulib_cdd_diff(&catalogue,&c,NULL,0,&n)==MULIB_CDD_LIMIT && n==6);
    assert(!mulib_cdd_diff(&catalogue,&c,out,8,&n) && n==6);
    assert(out[0].kind==MULIB_CDD_REMOVED && out[1].kind==MULIB_CDD_CHANGED && out[5].kind==MULIB_CDD_ADDED);
    assert(out[0].before==1 && out[0].after==MULIB_CDD_NONE);
    assert(out[1].before==2 && out[1].after==0);
    assert(out[5].before==MULIB_CDD_NONE && out[5].after==2);
    out[1].before=MULIB_CDD_NONE;
    assert(mulib_cdd_diff(&catalogue,&c,out,1,&n)==MULIB_CDD_LIMIT && n==6);
    assert(out[0].before==1 && out[1].before==MULIB_CDD_NONE);
    assert(mulib_cdd_diff(&catalogue,&c,NULL,1,&n)==MULIB_CDD_INVALID && n==0);
    assert(mulib_cdd_diff(&catalogue,&c,out,8,NULL)==MULIB_CDD_INVALID && out[1].before==MULIB_CDD_NONE);
    changed[2].id=changed[1].id; n=9;
    assert(mulib_cdd_diff(&c,&catalogue,out,8,&n)==MULIB_CDD_DUPLICATE && n==0);
    n=9;
    assert(mulib_cdd_diff(&catalogue,&c,out,8,&n)==MULIB_CDD_DUPLICATE && n==0);
    assert(out[0].before==1 && out[1].before==MULIB_CDD_NONE);
    mulib_cdd_entry_t swapped[]={entries[2],entries[1],entries[0]};
    mulib_cdd_catalogue_t before={entries,3},after={swapped,3};
    assert(mulib_cdd_diff(&before,&after,NULL,0,&n)==MULIB_CDD_OK && n==0);
}
/* Independent brute-force permutation oracle: each bit denotes one concrete
 * capability revision; a requirement is a mask of acceptable revisions.
 * No subset memo, relevance pruning, cost ordering, or CDD helper is used. */
struct oracle_case {
    unsigned n, allowed, initial, limit, goals[2];
    unsigned provides[6], needs[6][2], conflicts[6], costs[6];
};
static int oracle_has(unsigned caps,unsigned requirement) {
    return !requirement || (caps & requirement)!=0;
}
static int oracle_step(const struct oracle_case *o,unsigned i,unsigned selected,unsigned caps) {
    if (i>=o->n || !(o->allowed & (1u<<i)) || (selected & (1u<<i)) ||
        !oracle_has(caps,o->needs[i][0]) || !oracle_has(caps,o->needs[i][1]) ||
        (selected & o->conflicts[i])) return 0;
    for (unsigned j=0;j<o->n;j++) if ((selected & (1u<<j)) && (o->conflicts[j] & (1u<<i))) return 0;
    return 1;
}
static void brute(const struct oracle_case *o,unsigned selected,unsigned caps,unsigned cost,unsigned count,
                  unsigned *best,unsigned *bestn) {
    if (cost>o->limit) return;
    if (oracle_has(caps,o->goals[0]) && oracle_has(caps,o->goals[1]) &&
        (cost<*best || (cost==*best && count<*bestn))) {
        *best=cost; *bestn=count;
    }
    for (unsigned i=0;i<o->n;i++) if (oracle_step(o,i,selected,caps))
        brute(o,selected|(1u<<i),caps|o->provides[i],cost+o->costs[i],count+1,best,bestn);
}
static void oracle_check(const struct oracle_case *o,const mulib_cdd_catalogue_t *c,const mulib_cdd_problem_t *p) {
    unsigned best=1000,bestn=1000;
    brute(o,0,o->initial,0,0,&best,&bestn);
    mulib_cdd_plan_t plan;
    mulib_cdd_resolve_stats_t st;
    mulib_cdd_status_t rc=mulib_cdd_resolve(c,p,256,&plan,&st);
    if (best==1000) {
        assert(rc==MULIB_CDD_UNSAT && !plan.found && !st.optimal);
        return;
    }
    assert(rc==MULIB_CDD_OK && plan.found && st.optimal && plan.cost==best && plan.count==bestn);
    assert(mulib_cdd_plan_validate(c,p,&plan,NULL)==MULIB_CDD_OK);
    unsigned caps=o->initial,selected=0,cost=0;
    for (size_t k=0;k<plan.count;k++) {
        assert(plan.order[k]<o->n);
        unsigned i=(unsigned)plan.order[k];
        assert(oracle_step(o,i,selected,caps));
        selected|=1u<<i; caps|=o->provides[i]; cost+=o->costs[i];
    }
    assert(cost==best && oracle_has(caps,o->goals[0]) && oracle_has(caps,o->goals[1]));
}
static uint32_t seed=0x12345678u;
static unsigned rnd(unsigned n) { seed=seed*1664525u+1013904223u; return (seed>>8)%n; }
static void test_oracle(void) {
    static const char *names[]={"a","b","c","d","e","f","goal"};
    for(unsigned t=0;t<500;t++) {
        mulib_cdd_entry_t e[6]; mulib_cdd_cap_t cap[6]; mulib_cdd_req_t req[6]; const char *conf[6];
        struct oracle_case o={0}; o.n=6; o.goals[0]=64u;
        memset(e,0,sizeof e);
        for(unsigned i=0;i<6;i++) {
            unsigned k=rnd(7),j=rnd(8),h=rnd(8);
            o.provides[i]=1u<<k; o.needs[i][0]=j<7?1u<<j:0; o.conflicts[i]=(h<6 && h!=i)?1u<<h:0; o.costs[i]=rnd(6);
            cap[i].name=names[k]; cap[i].revision=1;
            req[i].name=j<7?names[j]:NULL; req[i].min_revision=1; req[i].max_revision=0;
            conf[i]=h<6?names[h]:NULL;
            e[i].id=names[i]; e[i].category="test"; e[i].implementation="test"; e[i].revision=1; e[i].cost=o.costs[i]; e[i].provides=cap+i; e[i].provide_count=1;
            e[i].requirements=req+i; e[i].require_count=o.needs[i][0]?1:0; e[i].conflicts=conf+i; e[i].conflict_count=o.conflicts[i]?1:0;
        }
        o.allowed=rnd(64); o.limit=rnd(20);
        unsigned initial=rnd(7); o.initial=1u<<initial;
        mulib_cdd_req_t goal={"goal",1,0}; mulib_cdd_cap_t avail={names[initial],1};
        mulib_cdd_catalogue_t c={e,6}; mulib_cdd_problem_t p={&goal,1,&avail,1,o.allowed,o.limit};
        oracle_check(&o,&c,&p);
    }
}
static void test_exhaustive_oracle(void) {
    static const mulib_cdd_cap_t caps[]={{"x",1},{"x",2},{"y",1}};
    static const mulib_cdd_req_t reqs[][2]={
        {{NULL,0,0},{NULL,0,0}}, {{"x",1,0},{NULL,0,0}},
        {{"x",2,2},{NULL,0,0}}, {{"x",1,1},{"y",1,0}}
    };
    static const unsigned masks[][2]={{0,0},{3,0},{2,0},{1,4}};
    static const size_t counts[]={0,1,1,2};
    static const char *ids[]={"a","b","c"};
    static const unsigned costs[][3]={{0,0,0},{3,0,1}};
    static const unsigned limits[]={0,3};
    for (unsigned requirements=0;requirements<64;requirements++)
    for (unsigned allowed=0;allowed<8;allowed++)
    for (unsigned conflict=0;conflict<3;conflict++)
    for (unsigned priced=0;priced<2;priced++)
    for (unsigned initial=0;initial<4;initial++)
    for (unsigned major=1;major<=2;major++) {
        struct oracle_case o={0};
        o.n=3; o.allowed=allowed; o.initial=initial?1u<<(initial-1):0;
        o.limit=limits[priced]; o.goals[0]=major; o.goals[1]=4;
        mulib_cdd_entry_t e[3];
        /* c supplies both x@2 and y; a/b supply one revision of x each. */
        for (unsigned i=0;i<3;i++) {
            unsigned r=(requirements>>(2*i))&3u;
            o.provides[i]=i==2?6u:1u<<i;
            o.needs[i][0]=masks[r][0]; o.needs[i][1]=masks[r][1];
            o.costs[i]=costs[priced][i];
            mulib_cdd_entry_t entry={ids[i],"test","label",1,o.costs[i],
                caps+(i==2?1:i),i==2?2u:1u,reqs[r],counts[r],NULL,0,NULL,0};
            e[i]=entry;
        }
        if (conflict) {
            unsigned from=conflict==1?0:2,to=conflict==1?2:0;
            e[from].conflicts=ids+to; e[from].conflict_count=1;
            o.conflicts[from]=1u<<to;
        }
        mulib_cdd_req_t goals[]={{"x",major,major},{"y",1,0}};
        mulib_cdd_problem_t p={goals,2,initial?caps+initial-1:NULL,initial?1u:0u,o.allowed,o.limit};
        mulib_cdd_catalogue_t c={e,3};
        oracle_check(&o,&c,&p);
    }
}
static void test_multigoal(void) {
    static const mulib_cdd_cap_t both[]={{"g1",1},{"g2",2}}, one[]={{"g1",1}}, two[]={{"g2",2}}, wrong[]={{"g2",3}};
    static const mulib_cdd_req_t goals[]={{"g1",1,1},{"g2",2,2}};
    const mulib_cdd_entry_t e[]={
      {"one","test","x",1,2,one,1,NULL,0,NULL,0,NULL,0},
      {"two","test","x",1,2,two,1,NULL,0,NULL,0,NULL,0},
      {"both","test","x",1,4,both,2,NULL,0,NULL,0,NULL,0},
      {"wrong","test","x",1,0,wrong,1,NULL,0,NULL,0,NULL,0}
    };
    mulib_cdd_catalogue_t c={e,4}; mulib_cdd_problem_t p={goals,2,NULL,0,UINT64_MAX,UINT64_MAX};
    mulib_cdd_plan_t plan;
    assert(!mulib_cdd_resolve(&c,&p,100,&plan,NULL) && plan.count==1 && plan.cost==4 && plan.order[0]==2);
    p.allowed=11; assert(!mulib_cdd_resolve(&c,&p,100,&plan,NULL) && plan.count==2 && plan.cost==4);
    p.allowed=9; assert(mulib_cdd_resolve(&c,&p,100,&plan,NULL)==MULIB_CDD_UNSAT);
    p.goal_count=0; p.max_cost=0; assert(!mulib_cdd_resolve(&c,&p,1,&plan,NULL) && plan.count==0);

    /* Equal objective: stable candidate traversal by ID, not input order. */
    mulib_cdd_entry_t tied[]={e[2],e[2]};
    tied[1].id="alternate";
    c.entries=tied; c.count=2;
    p.goal_count=2; p.allowed=UINT64_MAX; p.max_cost=4;
    assert(mulib_cdd_resolve(&c,&p,10,&plan,NULL)==MULIB_CDD_OK && plan.order[0]==1);
    mulib_cdd_entry_t save=tied[0]; tied[0]=tied[1]; tied[1]=save;
    assert(mulib_cdd_resolve(&c,&p,10,&plan,NULL)==MULIB_CDD_OK && plan.order[0]==0);
}
static void test_64(void) {
    mulib_cdd_entry_t e[64]; char names[64][8]; memset(e,0,sizeof e);
    for(size_t i=0;i<64;i++) { snprintf(names[i],8,"e%02u",(unsigned)i); e[i].id=names[i]; e[i].category="test"; e[i].implementation="x"; e[i].revision=1; }
    e[63].provides=output; e[63].provide_count=1; e[63].cost=3;
    mulib_cdd_catalogue_t c={e,64}; mulib_cdd_problem_t p=problem; p.allowed=UINT64_C(1)<<63;
    mulib_cdd_plan_t plan; assert(!mulib_cdd_resolve(&c,&p,5,&plan,NULL) && plan.count==1 && plan.order[0]==63);
    assert(mulib_cdd_plan_validate(&c,&p,&plan,NULL)==MULIB_CDD_OK);
    {
        mulib_cdd_registry_t reg; mulib_cdd_registry_init(&reg);
        for (size_t i=0;i<64;i++) assert(mulib_cdd_registry_add(&reg,e+i)==MULIB_CDD_OK);
        assert(mulib_cdd_registry_add(&reg,entries)==MULIB_CDD_LIMIT);
        mulib_cdd_catalogue_t view=mulib_cdd_registry_view(&reg);
        assert(view.count==64 && mulib_cdd_find(&view,names[63])==63);
        assert(mulib_cdd_registry_remove(&reg,names[0])==MULIB_CDD_OK);
        view=mulib_cdd_registry_view(&reg);
        assert(view.count==63 && mulib_cdd_find(&view,names[63])==62);
        assert(mulib_cdd_registry_add(&reg,entries)==MULIB_CDD_OK);
    }
    c.count=65;
    assert(mulib_cdd_validate(&c,NULL)==MULIB_CDD_LIMIT);
    assert(mulib_cdd_find(&c,"missing")==MULIB_CDD_NONE);
    assert(!mulib_cdd_satisfied(&c,&p,UINT64_MAX,need_ir) && !mulib_cdd_can_append(&c,&p,0,63));
    mulib_cdd_resolve_stats_t st={7,8,1};
    assert(mulib_cdd_resolve(&c,&p,5,&plan,&st)==MULIB_CDD_INVALID);
    assert(!st.expanded && !st.memo_hits && !st.optimal && !plan.found && !plan.cost && !plan.count);
    for (size_t i=0;i<64;i++) assert(!plan.order[i]);
    size_t n=9;
    const mulib_cdd_query_t q={NULL,NULL,0,NULL,0};
    assert(mulib_cdd_query_run(&c,&q,NULL,0,&n)==MULIB_CDD_LIMIT && n==0);
    n=9;
    assert(mulib_cdd_diff(&catalogue,&c,NULL,0,&n)==MULIB_CDD_LIMIT && n==0);

    mulib_cdd_cap_t cap[64];
    mulib_cdd_req_t req[64];
    for (size_t i=0;i<64;i++) {
        mulib_cdd_cap_t supplied={names[i],1}; cap[i]=supplied;
        mulib_cdd_req_t needed={names[i],1,1}; req[i]=needed;
        e[i].provides=cap+i; e[i].provide_count=1; e[i].cost=UINT32_MAX;
        if (i) { e[i].requirements=req+i-1; e[i].require_count=1; }
    }
    c.count=64; p.allowed=UINT64_MAX; p.goals=req+63; p.max_cost=UINT64_C(64)*UINT32_MAX;
    assert(mulib_cdd_resolve(&c,&p,65,&plan,&st)==MULIB_CDD_OK);
    assert(st.optimal && st.expanded==65 && plan.count==64 && plan.cost==p.max_cost);
    for (size_t i=0;i<64;i++) assert(plan.order[i]==i);
    assert(mulib_cdd_plan_validate(&c,&p,&plan,NULL)==MULIB_CDD_OK);
    p.max_cost--;
    assert(mulib_cdd_resolve(&c,&p,65,&plan,&st)==MULIB_CDD_UNSAT && !plan.found && !st.optimal);
}
int main(void) {
    test_catalogue(); test_catalogue_failures(); test_registry_borrowing();
    test_query_schema(); test_query_schema_failures();
    test_resolver(); test_budget(); test_plan_failures();
    test_exec_diff(); test_oracle(); test_exhaustive_oracle();
    test_multigoal(); test_64();
    puts("CDD tests passed (500 seeded + 24,576 exhaustive oracle cases)");
    return 0;
}
