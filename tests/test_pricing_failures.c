/* Fail each successive allocation and verify rollback/output contracts. */
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
static size_t allocation,fail_at;
static int fail(void){return ++allocation==fail_at;}
static void *test_malloc(size_t n){return fail()?NULL:malloc(n);}
static void *test_calloc(size_t n,size_t size){return fail()?NULL:calloc(n,size);}
static void *test_realloc(void *p,size_t n){return fail()?NULL:realloc(p,n);}
#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define MULIB_IMPL_STATSPROC
#define MULIB_IMPL_STATSPRICE
#define MULIB_IMPL_COMPETPRICE
#define MULIB_IMPL_HISTPRICE
#define MULIB_IMPL_SAT
#define MULIB_IMPL_SMT
#include "mulib-statsprice.h"
#include "mulib-competprice.h"
#include "mulib-histprice.h"
#include "mulib-smt.h"
#undef malloc
#undef calloc
#undef realloc

static void reset(size_t n){allocation=0;fail_at=n;}
int main(void) {
  const double prices[]={1,2,3,4,5,6},weights[]={1,1,1,1,1,1};
  const int clause[]={1};size_t n;int success;
  for(n=1;n<30;++n) {
    mulib_statsprice_summary_t summary={111,222,333,444,555,666};
    reset(n);success=mulib_statsprice_summarize(prices,6,&summary);
    if(success)break;
    assert(summary.median==111&&summary.mean==222&&summary.huber==666);
  }
  assert(n<30);
  for(n=1;n<30;++n) {
    double result=1234;
    reset(n);success=mulib_statsprice_weighted(prices,weights,6,1,&result);
    if(success)break;
    assert(result==1234);
  }
  assert(n<30);
  for(n=1;n<10;++n) {
    double slope=1234,intercept=5678;
    reset(n);success=mulib_statsproc_theil_sen(prices,prices,6,&slope,&intercept);
    if(success)break;
    assert(slope==1234&&intercept==5678);
  }
  assert(n<10);
  for(n=1;n<10;++n) {
    const mulib_competprice_rival_t rival={10,1,1,1};
    mulib_competprice_config_t options={0};mulib_competprice_result_t result={0};
    options.max_price=100;result.price=1234;
    reset(n);success=mulib_competprice_recommend(&rival,1,&options,&result);
    if(success)break;
    assert(result.price==1234);
  }
  assert(n<10);
  {mulib_histprice_forecast_t result={1234,5678};
    reset(1);assert(!mulib_histprice_seasonal(prices,6,2,.5,.5,.5,1,&result));
    assert(result.point==1234&&result.variance==5678);
  }
  for(n=1;n<10;++n) {
    mulib_sat_t *solver;
    reset(n);solver=mulib_sat_create(1,0);
    if(solver){mulib_sat_destroy(solver);break;}
  }
  assert(n<10);
  for(n=1;n<10;++n) {
    mulib_sat_t *solver;
    reset(0);solver=mulib_sat_create(1,0);assert(solver);
    assert(mulib_sat_solve(solver,NULL,0)==MULIB_SAT_SAT);
    reset(n);success=mulib_sat_add_clause(solver,clause,1);
    if(!success)assert(mulib_sat_value(solver,1)==-1);
    else assert(!mulib_sat_value(solver,1));
    reset(0);assert(mulib_sat_solve(solver,NULL,0)==MULIB_SAT_SAT);
    assert(mulib_sat_value(solver,1)==(success?1:-1));
    mulib_sat_destroy(solver);if(success)break;
  }
  assert(n<10);
  for(n=1;n<10;++n) {
    mulib_smt_t *solver;
    reset(n);solver=mulib_smt_create(1,2,10);
    if(solver){mulib_smt_destroy(solver);break;}
  }
  assert(n<10);
  for(n=1;n<15;++n) {
    mulib_smt_t *solver;int status;
    reset(0);solver=mulib_smt_create(1,1,10);assert(solver);
    /* The first Boolean model is false, and has an inconsistent guarded edge.
     * This exercises allocation of the permanent theory blocking clause. */
    assert(mulib_smt_difference(solver,0,0,-1,-1));
    reset(n);status=mulib_smt_solve(solver);
    assert(status==MULIB_SMT_SAT||status==MULIB_SMT_MEMORY);
    if(status==MULIB_SMT_MEMORY)assert(!mulib_smt_boolean_value(solver,1));
    reset(0);assert(mulib_smt_solve(solver)==MULIB_SMT_SAT);
    assert(mulib_smt_boolean_value(solver,1)==1);
    mulib_smt_destroy(solver);if(status==MULIB_SMT_SAT)break;
  }
  assert(n<15);
  puts("pricing/solver allocation-failure rollback passed");
  return 0;
}
