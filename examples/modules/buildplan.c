#define MULIB_IMPL_BUILDPLAN
#include "mulib-buildplan.h"
#include <assert.h>
#include <stdio.h>

static int print_wave(void *user, const size_t *ids, size_t count) {
  const mulib_buildplan_t *plan = (const mulib_buildplan_t *)user;
  size_t i;
  for (i = 0; i < count; ++i)
    printf("%s%s", i ? " " : "", mulib_buildplan_at(plan, ids[i])->name);
  putchar('\n');
  return 0;
}

int main(void) {
  mulib_buildplan_t *plan = mulib_buildplan_create(3, 2);
  mulib_buildplan_budget_t budget = {2, 0};
  size_t library, app, tests;
  assert(plan);
  assert(!mulib_buildplan_target(plan, "library", &library));
  assert(!mulib_buildplan_target(plan, "app", &app));
  assert(!mulib_buildplan_target(plan, "tests", &tests));
  assert(!mulib_buildplan_depend(plan, library, app));
  assert(!mulib_buildplan_depend(plan, library, tests));
  assert(!mulib_buildplan_set_resources(plan, app, 2, 0, 0));
  assert(!mulib_buildplan_set_resources(plan, tests, 1, 0, 1));
  assert(!mulib_buildplan_schedule(plan, budget, print_wave, plan));
  mulib_buildplan_destroy(plan);
  return 0;
}
