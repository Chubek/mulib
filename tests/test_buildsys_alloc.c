#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static long allocation_budget = -1;

static void *
test_malloc(size_t size)
{
  if (!allocation_budget)
    return NULL;
  if (allocation_budget > 0)
    --allocation_budget;
  return malloc(size);
}

static void *
test_calloc(size_t count, size_t size)
{
  void *memory;
  if (size && count > SIZE_MAX / size)
    return NULL;
  memory = test_malloc(count * size);
  if (memory)
    memset(memory, 0, count * size);
  return memory;
}

static void *
test_realloc(void *memory, size_t size)
{
  if (!allocation_budget)
    return NULL;
  if (allocation_budget > 0)
    --allocation_budget;
  return realloc(memory, size);
}

#define malloc test_malloc
#define calloc test_calloc
#define realloc test_realloc
#define MULIB_IMPL_SHA
#define MULIB_IMPL_BUILDPLAN
#define MULIB_IMPL_DEPFILE
#define MULIB_IMPL_BUILDPARSE
#define MULIB_IMPL_BUILDSTATE
#include "mulib-buildparse.h"
#include "mulib-buildstate.h"
#include "mulib-depfile.h"
#undef malloc
#undef calloc
#undef realloc

#include <assert.h>

static int
accept_rule(void *user, const mulib_depfile_rule_t *rule)
{
  (void)user;
  (void)rule;
  return 0;
}

static void
plan_allocations(void)
{
  long budget;
  int succeeded = 0;
  for (budget = 0; budget < 16; ++budget) {
    mulib_buildplan_t *plan;
    allocation_budget = budget;
    plan = mulib_buildplan_create(2, 1);
    allocation_budget = -1;
    if (plan) {
      succeeded = 1;
      mulib_buildplan_destroy(plan);
      break;
    }
  }
  assert(succeeded);
}

static void
parser_allocations(void)
{
  static const char source[] = "add_executable(app main.c)\n";
  long budget;
  int succeeded = 0;
  for (budget = 0; budget < 64; ++budget) {
    mulib_buildplan_t *plan = NULL;
    int rc;
    allocation_budget = budget;
    rc = mulib_buildparse(source, sizeof(source) - 1,
                          MULIB_BUILDPARSE_CMAKE, NULL, &plan, NULL);
    allocation_budget = -1;
    if (!rc) {
      succeeded = 1;
      mulib_buildplan_destroy(plan);
      break;
    }
    assert(rc == MULIB_BUILDPARSE_MEMORY && !plan);
  }
  assert(succeeded);
}

static void
depfile_allocations(void)
{
  static const char source[] = "main.o: main.c api.h\n";
  long budget;
  int succeeded = 0;
  for (budget = 0; budget < 64; ++budget) {
    int rc;
    allocation_budget = budget;
    rc = mulib_depfile_parse(source, sizeof(source) - 1, NULL, accept_rule,
                             NULL, NULL);
    allocation_budget = -1;
    if (!rc) {
      succeeded = 1;
      break;
    }
    assert(rc == MULIB_DEPFILE_MEMORY);
  }
  assert(succeeded);
}

static void
state_allocations(void)
{
  const char *argv[] = {"cc", "main.c"};
  const char *inputs[] = {"main.c", "api.h"};
  const char *outputs[] = {"main.o"};
  const mulib_buildstate_env_t env[] = {{"LANG", "C"}};
  const mulib_buildstate_action_t action = {
      argv, 2, NULL, env, 1, inputs, 2, outputs, 1, 0, 0};
  char fingerprint[65];
  long budget;
  int succeeded = 0;
  for (budget = 0; budget < 16; ++budget) {
    int rc;
    allocation_budget = budget;
    rc = mulib_buildstate_action_fingerprint(&action, fingerprint);
    allocation_budget = -1;
    if (!rc) {
      succeeded = 1;
      break;
    }
    assert(rc == MULIB_BUILDSTATE_MEMORY);
  }
  assert(succeeded);
}

int
main(void)
{
  plan_allocations();
  parser_allocations();
  depfile_allocations();
  state_allocations();
  return 0;
}
