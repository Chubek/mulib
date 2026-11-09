#define MULIB_IMPL_SAT
#define MULIB_IMPL_SMT
#include "mulib-smt.h"
#include <assert.h>
#include <stdio.h>
#include <float.h>
int main(void) {
  double x,y;
  mulib_smt_t *s=mulib_smt_create(1,2,100);
  assert(s);
  /* If Boolean b then x-y <= -3; if !b then y-x <= -3. */
  assert(mulib_smt_difference(s,0,1,-3,1));
  assert(mulib_smt_difference(s,1,0,-3,-1));
  assert(mulib_smt_solve(s)==MULIB_SMT_SAT);
  assert(mulib_smt_real_value(s,0,&x)&&mulib_smt_real_value(s,1,&y));
  if(mulib_smt_boolean_value(s,1)==1)assert(x-y<=-3);
  else assert(y-x<=-3);
  /* Force b=true then impose x-y>=1: b=true becomes theory-infeasible,
     but b=false remains feasible and requires Boolean-theory search. */
  assert(mulib_smt_difference(s,1,0,-1,1));
  assert(mulib_smt_solve(s)==MULIB_SMT_SAT);
  assert(mulib_smt_boolean_value(s,1)==-1);
  assert(mulib_smt_clause(s,(int[]){1},1));
  assert(mulib_smt_solve(s)==MULIB_SMT_UNSAT);
  assert(!mulib_smt_real_value(s,0,&x));
  mulib_smt_destroy(s);
  s=mulib_smt_create(0,3,10);assert(s);
  assert(mulib_smt_difference(s,1,0,-1e16,0));
  assert(mulib_smt_difference(s,2,1,-1,0));
  /* Long-double distances are feasible, but rounding them loses the -1 edge. */
  assert(mulib_smt_solve(s)==MULIB_SMT_LIMIT);
  x=1234;assert(!mulib_smt_real_value(s,0,&x)&&x==1234);
  assert(mulib_smt_solve(s)==MULIB_SMT_LIMIT); /* not permanently blocked */
  mulib_smt_destroy(s);
  s=mulib_smt_create(0,3,10);assert(s);
  assert(mulib_smt_difference(s,1,0,-DBL_MAX,0));
  assert(mulib_smt_difference(s,2,1,-DBL_MAX,0));
  assert(mulib_smt_solve(s)==MULIB_SMT_LIMIT); /* numeric range, not OOM */
  mulib_smt_destroy(s);
  s=mulib_smt_create(0,1,10);assert(s);
  assert(mulib_smt_difference(s,0,0,-1,0));
  assert(mulib_smt_solve(s)==MULIB_SMT_UNSAT);
  mulib_smt_destroy(s);
  puts("smt difference logic passed");return 0;
}
