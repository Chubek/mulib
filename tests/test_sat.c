#define MULIB_IMPL_SAT
#include "mulib-sat.h"
#include <assert.h>
#include <stdio.h>
#include <limits.h>
int main(void) {
  mulib_sat_t *s=mulib_sat_create(3,0);
  const int a[]={1,2},b[]={-1,2},c[]={1,-2};
  const int contradiction[]={-2};
  assert(s&&mulib_sat_variables(s)==3);
  assert(mulib_sat_add_clause(s,a,2));
  assert(mulib_sat_add_clause(s,b,2));
  assert(mulib_sat_add_clause(s,c,2));
  assert(mulib_sat_solve(s,NULL,0)==MULIB_SAT_SAT);
  assert(mulib_sat_value(s,1)==1&&mulib_sat_value(s,2)==1);
  assert(mulib_sat_solve(s,contradiction,1)==MULIB_SAT_UNSAT);
  assert(!mulib_sat_value(s,1));
  assert(!mulib_sat_add_clause(s,(int[]){4},1));
  assert(mulib_sat_add_clause(s,contradiction,1));
  assert(mulib_sat_solve(s,NULL,0)==MULIB_SAT_UNSAT);
  mulib_sat_destroy(s);
  s=mulib_sat_create(4096,1);assert(s);
  assert(mulib_sat_solve(s,NULL,0)==MULIB_SAT_SAT); /* no unnecessary decisions */
  assert(mulib_sat_value(s,4096)==-1);
  assert(mulib_sat_solve(s,NULL,1)==MULIB_SAT_INVALID);
  assert(mulib_sat_value(s,4096)==0);
  assert(mulib_sat_solve(s,(int[]){1,-1,INT_MIN},3)==MULIB_SAT_INVALID);
  assert(!mulib_sat_add_clause(s,(int[]){INT_MIN},1));
  assert(mulib_sat_add_clause(s,(int[]){1,2},2));
  assert(mulib_sat_add_clause(s,(int[]){3,4},2));
  assert(mulib_sat_solve(s,NULL,0)==MULIB_SAT_LIMIT);
  assert(!mulib_sat_value(s,1));
  assert(mulib_sat_solve(s,(int[]){1},1)==MULIB_SAT_SAT);
  mulib_sat_destroy(s);
  s=mulib_sat_create(0,0);assert(s);
  assert(mulib_sat_solve(s,NULL,0)==MULIB_SAT_SAT);
  assert(mulib_sat_add_clause(s,NULL,0));
  assert(mulib_sat_solve(s,NULL,0)==MULIB_SAT_UNSAT);
  mulib_sat_destroy(s);
  puts("sat passed");return 0;
}
