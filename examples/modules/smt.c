#define MULIB_IMPL_SAT
#define MULIB_IMPL_SMT
#include "mulib-smt.h"
#include <assert.h>

int main(void) {
  mulib_smt_t *solver=mulib_smt_create(1,2,100);
  const int enabled[]={1};double x,y;
  assert(solver&&mulib_smt_difference(solver,0,1,-3,1));
  assert(mulib_smt_clause(solver,enabled,1));
  assert(mulib_smt_solve(solver)==MULIB_SMT_SAT);
  assert(mulib_smt_real_value(solver,0,&x)&&mulib_smt_real_value(solver,1,&y));
  assert(x-y<=-3);
  mulib_smt_destroy(solver);
  return 0;
}
