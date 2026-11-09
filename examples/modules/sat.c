#define MULIB_IMPL_SAT
#include "mulib-sat.h"
#include <assert.h>

int main(void) {
  mulib_sat_t *solver=mulib_sat_create(2,0);
  const int a[]={1,2},b[]={-1,2};
  assert(solver&&mulib_sat_add_clause(solver,a,2));
  assert(mulib_sat_add_clause(solver,b,2));
  assert(mulib_sat_solve(solver,NULL,0)==MULIB_SAT_SAT);
  assert(mulib_sat_value(solver,2)==1);
  mulib_sat_destroy(solver);
  return 0;
}
