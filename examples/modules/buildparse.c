#define MULIB_IMPL_BUILDPLAN
#define MULIB_IMPL_BUILDPARSE
#include "mulib-buildparse.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  const char text[] =
      "add_library(core STATIC core.c)\n"
      "add_executable(app main.c)\n"
      "add_dependencies(app core)\n";
  mulib_buildplan_t *plan = NULL;
  mulib_buildparse_error_t error;
  size_t order[4], i;
  assert(!mulib_buildparse(text, strlen(text), MULIB_BUILDPARSE_CMAKE,
                           NULL, &plan, &error));
  assert(!mulib_buildplan_order(plan, order, 4));
  for (i = 0; i < mulib_buildplan_count(plan); ++i)
    puts(mulib_buildplan_at(plan, order[i])->name);
  mulib_buildplan_destroy(plan);
  return 0;
}
