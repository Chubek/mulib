#define MULIB_IMPL_CASSOWARY_SOLVER
#include "mulib-cassowary-solver.h"
#include <assert.h>
int main(void) {
  mulib_cassowary_solver_t *solver = mulib_cassowary_create(NULL);
  mulib_cassowary_term_t term; size_t width, id; double value;
  assert(solver && !mulib_cassowary_add_variable(solver, &width)); term.variable = width; term.coefficient = 1;
  assert(!mulib_cassowary_add(solver, &term, 1, MULIB_CASSOWARY_GE, 100, MULIB_CASSOWARY_REQUIRED, &id));
  assert(!mulib_cassowary_add_edit(solver, width, MULIB_CASSOWARY_STRONG));
  assert(!mulib_cassowary_suggest(solver, width, 50));
  assert(!mulib_cassowary_value(solver, width, &value) && value == 100);
  mulib_cassowary_destroy(solver); return 0;
}
