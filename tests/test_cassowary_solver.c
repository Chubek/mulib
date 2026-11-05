#define MULIB_IMPL_CASSOWARY_SOLVER
#include "mulib-cassowary-solver.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static double value(mulib_cassowary_solver_t *s, size_t v) { double x; assert(!mulib_cassowary_value(s, v, &x)); return x; }
static size_t constraint(mulib_cassowary_solver_t *s, size_t v, mulib_cassowary_relation_t rel, double rhs, double strength) {
  mulib_cassowary_term_t term = {v, 1}; size_t id; int rc = mulib_cassowary_add(s, &term, 1, rel, rhs, strength, &id);
  if (rc) fprintf(stderr, "cassowary add: %d\n", rc);
  assert(!rc); return id;
}
static int oracle(void) {
  size_t variables, constraints, i, j, id; int status = 0;
  mulib_cassowary_solver_t *s = mulib_cassowary_create(NULL);
  if (!s || scanf("%zu %zu", &variables, &constraints) != 2 || variables > 16 || constraints > 64) return 1;
  for (i = 0; i < variables; ++i) assert(!mulib_cassowary_add_variable(s, &id));
  for (i = 0; i < constraints; ++i) {
    mulib_cassowary_term_t terms[16]; double rhs, strength; int relation;
    for (j = 0; j < variables; ++j) { terms[j].variable = j; assert(scanf("%lf", &terms[j].coefficient) == 1); }
    assert(scanf("%d %lf %lf", &relation, &rhs, &strength) == 3);
    if (!status) status = mulib_cassowary_add(s, terms, variables, (mulib_cassowary_relation_t)relation, rhs, strength, &id);
  }
  printf("%d", status);
  for (i = 0; i < variables; ++i) printf(" %.17g", value(s, i));
  putchar('\n'); mulib_cassowary_destroy(s); return 0;
}
int main(int argc, char **argv) {
  mulib_cassowary_solver_t *s = mulib_cassowary_create(NULL); size_t x, y, upper, id, before;
  mulib_cassowary_term_t relation[2]; double old;
  if (argc == 2 && !strcmp(argv[1], "--oracle")) { mulib_cassowary_destroy(s); return oracle(); }
  assert(s && !mulib_cassowary_add_variable(s, &x) && !mulib_cassowary_add_variable(s, &y));
  constraint(s, x, MULIB_CASSOWARY_GE, 0, MULIB_CASSOWARY_REQUIRED);
  upper = constraint(s, x, MULIB_CASSOWARY_LE, 100, MULIB_CASSOWARY_REQUIRED);
  relation[0].variable = y; relation[0].coefficient = 1; relation[1].variable = x; relation[1].coefficient = -1;
  assert(!mulib_cassowary_add(s, relation, 2, MULIB_CASSOWARY_EQ, 20, 0, &id));
  assert(!mulib_cassowary_add_edit(s, x, MULIB_CASSOWARY_STRONG));
  assert(!mulib_cassowary_suggest(s, x, 150));
  assert(fabs(value(s, x) - 100) < 1e-7 && fabs(value(s, y) - 120) < 1e-7);
  constraint(s, y, MULIB_CASSOWARY_EQ, 0, MULIB_CASSOWARY_WEAK);
  assert(fabs(value(s, y) - 120) < 1e-7);
  before = mulib_cassowary_constraint_count(s); old = value(s, x);
  { mulib_cassowary_term_t t = {x, 1}; id = 999;
    assert(mulib_cassowary_add(s, &t, 1, MULIB_CASSOWARY_LE, -1, 0, &id) == MULIB_CASSOWARY_UNSATISFIABLE);
    assert(id == 999 && value(s, x) == old && mulib_cassowary_constraint_count(s) == before);
  }
  assert(!mulib_cassowary_remove(s, upper));
  assert(fabs(value(s, x) - 150) < 1e-7 && fabs(value(s, y) - 170) < 1e-7);
  assert(!mulib_cassowary_suggest(s, x, -50)); assert(fabs(value(s, x)) < 1e-7);
  assert(!mulib_cassowary_remove_edit(s, x));
  assert(mulib_cassowary_suggest(s, x, 2) == MULIB_CASSOWARY_INVALID);
  mulib_cassowary_destroy(s);
  s = mulib_cassowary_create(NULL); assert(s && !mulib_cassowary_add_variable(s, &x));
  constraint(s, x, MULIB_CASSOWARY_GE, 10, 0);
  constraint(s, x, MULIB_CASSOWARY_EQ, 10, 0);
  constraint(s, x, MULIB_CASSOWARY_EQ, 10, 0); /* redundant required equality */
  assert(fabs(value(s, x) - 10) < 1e-7);
  { size_t bad; assert(mulib_cassowary_add(s, NULL, 0, MULIB_CASSOWARY_EQ, 1, 0, &bad) == MULIB_CASSOWARY_UNSATISFIABLE); }
  mulib_cassowary_destroy(s);
  { mulib_cassowary_options_t o = {1, 1, 1, 1, 0}; s = mulib_cassowary_create(&o); assert(s);
    assert(!mulib_cassowary_add_variable(s, &x)); assert(mulib_cassowary_add_variable(s, &y) == MULIB_CASSOWARY_LIMIT);
    { mulib_cassowary_term_t t = {x, 1}; assert(mulib_cassowary_add(s, &t, 1, MULIB_CASSOWARY_EQ, 1, 0, &id) == MULIB_CASSOWARY_LIMIT); }
    mulib_cassowary_destroy(s);
  }
  puts("cassowary tableau/edit/rollback passed"); return 0;
}
