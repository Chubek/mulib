#define MULIB_IMPL_SHA
#define MULIB_IMPL_BUILDPLAN
#define MULIB_IMPL_DEPFILE
#define MULIB_IMPL_BUILDPARSE
#define MULIB_IMPL_BUILDSTATE
#include "mulib-buildparse.h"
#include "mulib-buildstate.h"
#include "mulib-depfile.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct wave_log {
  size_t count;
  size_t lengths[8];
  char names[8][4][32];
  mulib_buildplan_t *plan;
} wave_log_t;

static int
record_wave(void *context, const size_t *ids, size_t count)
{
  wave_log_t *log = (wave_log_t *)context;
  size_t i, ignored;
  assert(log->count < 8 && count <= 4);
  log->lengths[log->count] = count;
  for (i = 0; i < count; ++i) {
    const char *name = mulib_buildplan_at(log->plan, ids[i])->name;
    assert(strlen(name) < sizeof(log->names[0][0]));
    strcpy(log->names[log->count][i], name);
  }
  assert(mulib_buildplan_target(log->plan, "busy", &ignored)
         == MULIB_BUILDPLAN_BUSY);
  ++log->count;
  return 0;
}

static int
has_dependency(const mulib_buildplan_t *plan, const char *dependency,
               const char *dependent)
{
  size_t before, after, i;
  assert(!mulib_buildplan_find(plan, dependency, &before));
  assert(!mulib_buildplan_find(plan, dependent, &after));
  for (i = 0; i < mulib_buildplan_dependency_count(plan); ++i) {
    size_t a, b;
    assert(!mulib_buildplan_dependency_at(plan, i, &a, &b));
    if (a == before && b == after)
      return 1;
  }
  return 0;
}

static void
plans(void)
{
  mulib_buildplan_t *plan = mulib_buildplan_create(8, 8);
  mulib_buildplan_t *cyclic = mulib_buildplan_create(3, 3);
  mulib_buildplan_t *full = mulib_buildplan_create(2, 0);
  mulib_buildplan_budget_t budget = {2, 128};
  wave_log_t log = {0};
  size_t app, lib, test, order[8], path[8], cycle[4], length;
  double total;
  assert(plan && cyclic && full);
  assert(!mulib_buildplan_target(plan, "app", &app));
  assert(!mulib_buildplan_target(plan, "lib", &lib));
  assert(!mulib_buildplan_target(plan, "test", &test));
  assert(!mulib_buildplan_target(plan, "app", &length) && length == app);
  assert(!mulib_buildplan_depend(plan, lib, app));
  assert(!mulib_buildplan_depend(plan, lib, test));
  assert(!mulib_buildplan_depend(plan, lib, test));
  assert(!mulib_buildplan_set_resources(plan, app, 2, 64, 0));
  assert(!mulib_buildplan_set_resources(plan, test, 1, 32, 1));
  assert(!mulib_buildplan_set_duration(plan, lib, 2.0));
  assert(!mulib_buildplan_set_duration(plan, app, 3.0));
  assert(!mulib_buildplan_set_duration(plan, test, 1.0));
  assert(!mulib_buildplan_order(plan, order, 8));
  assert(order[0] == lib && order[1] == app && order[2] == test);
  assert(!mulib_buildplan_critical_path(plan, path, 8, &length, &total));
  assert(length == 2 && path[0] == lib && path[1] == app && total == 5.0);
  log.plan = plan;
  assert(!mulib_buildplan_schedule(plan, budget, record_wave, &log));
  assert(log.count == 3);
  assert(log.lengths[0] == 1 && !strcmp(log.names[0][0], "lib"));
  assert(log.lengths[1] == 1 && !strcmp(log.names[1][0], "test"));
  assert(log.lengths[2] == 1 && !strcmp(log.names[2][0], "app"));
  assert(!mulib_buildplan_depend_names(cyclic, "a", "b"));
  assert(!mulib_buildplan_depend_names(cyclic, "b", "a"));
  assert(mulib_buildplan_validate(cyclic) == MULIB_BUILDPLAN_CYCLE);
  assert(mulib_buildplan_cycle(cyclic, NULL, 0, &length)
         == MULIB_BUILDPLAN_FULL);
  assert(length == 3);
  assert(!mulib_buildplan_cycle(cyclic, cycle, 4, &length));
  assert(length == 3 && cycle[0] == cycle[2]);
  assert(mulib_buildplan_depend_names(full, "one", "two")
         == MULIB_BUILDPLAN_FULL);
  assert(!mulib_buildplan_count(full));
  mulib_buildplan_destroy(full);
  mulib_buildplan_destroy(cyclic);
  mulib_buildplan_destroy(plan);
}

typedef struct depfile_log {
  size_t calls;
} depfile_log_t;

static int
depfile_rule(void *context, const mulib_depfile_rule_t *rule)
{
  depfile_log_t *log = (depfile_log_t *)context;
  if (!log->calls) {
    assert(rule->target_count == 1 && rule->dependency_count == 2);
    assert(!strcmp(rule->targets[0], "foo bar.o"));
    assert(!strcmp(rule->dependencies[0], "src file.c"));
    assert(!strcmp(rule->dependencies[1], "include.h"));
  } else {
    assert(rule->target_count == 2 && rule->dependency_count == 2);
    assert(!strcmp(rule->targets[0], "a") && !strcmp(rule->targets[1], "b"));
    assert(!strcmp(rule->dependencies[0], "one"));
    assert(!strcmp(rule->dependencies[1], "two"));
  }
  ++log->calls;
  return 0;
}

static int
ignore_depfile_rule(void *context, const mulib_depfile_rule_t *rule)
{
  (void)context;
  (void)rule;
  return 0;
}

static void
depfiles(void)
{
  const char source[] =
      "foo\\ bar.o: src\\ file.c include.h\n"
      "a b: one \\\n"
      " two\n";
  const char malformed[] = "target prerequisite";
  depfile_log_t log = {0};
  mulib_depfile_error_t error;
  mulib_depfile_limits_t limits = {0};
  size_t i;
  assert(!mulib_depfile_parse(source, sizeof(source) - 1, NULL, depfile_rule,
                              &log, &error));
  assert(log.calls == 2 && !error.message);
  assert(mulib_depfile_parse(malformed, sizeof(malformed) - 1, NULL,
                             depfile_rule, &log, &error)
         == MULIB_DEPFILE_INVALID);
  assert(error.line == 1 && error.message);
  limits.max_source_bytes = 2;
  assert(mulib_depfile_parse(source, sizeof(source) - 1, &limits,
                             depfile_rule, &log, NULL)
         == MULIB_DEPFILE_LIMIT);
  for (i = 0; i < sizeof(source); ++i) {
    int rc = mulib_depfile_parse(source, i, NULL, ignore_depfile_rule, NULL,
                                 NULL);
    assert(rc == MULIB_DEPFILE_OK || rc == MULIB_DEPFILE_INVALID);
  }
}

static void
check_parse(const char *source, mulib_buildparse_format_t format,
            const char *dependency, const char *dependent,
            const char *tag)
{
  mulib_buildplan_t *plan = NULL;
  mulib_buildparse_error_t error;
  size_t id;
  assert(!mulib_buildparse(source, strlen(source), format, NULL, &plan,
                           &error));
  assert(plan && has_dependency(plan, dependency, dependent));
  assert(!mulib_buildplan_find(plan, dependent, &id));
  assert(!strcmp(mulib_buildplan_at(plan, id)->tag, tag));
  mulib_buildplan_destroy(plan);
}

static void
parsers(void)
{
  const char cmake[] = "add_executable(app main.c)\n";
  const char bad[] = {'x', ':', ' ', 'y', '\0', 'z'};
  mulib_buildplan_t *plan = NULL;
  mulib_buildparse_options_t limits = {0};
  size_t i;
  check_parse("foo\\ bar.o: src\\ file.c include.h\n",
              MULIB_BUILDPARSE_MAKE, "src file.c", "foo bar.o", "make");
  check_parse("rule cc\nbuild app: cc main.c | generated.h\n",
              MULIB_BUILDPARSE_NINJA, "main.c", "app", "cc");
  check_parse("# add_executable(fake x)\n"
              "add_executable(app WIN32 main.c)\n"
              "target_sources(app PRIVATE generated.c)\n",
              MULIB_BUILDPARSE_CMAKE, "main.c", "app",
              "cmake:sources");
  check_parse("executable('app', dependencies: ['lib'])\n",
              MULIB_BUILDPARSE_MESON, "lib", "app", "meson:executable");
  check_parse("cc_binary(name = \"app\", srcs = [\"main.c\"], "
              "deps = [\":lib\"])\n",
              MULIB_BUILDPARSE_BAZEL, ":lib", "app", "bazel:cc_binary");
  assert(mulib_buildparse(bad, sizeof(bad), MULIB_BUILDPARSE_MAKE, NULL,
                          &plan, NULL)
         == MULIB_BUILDPARSE_INVALID);
  assert(!plan);
  limits.max_targets = 1;
  assert(mulib_buildparse("a: b\n", 5, MULIB_BUILDPARSE_MAKE, &limits,
                          &plan, NULL)
         == MULIB_BUILDPARSE_LIMIT);
  assert(!plan);
  for (i = 0; i < sizeof(cmake); ++i) {
    int rc = mulib_buildparse(cmake, i, MULIB_BUILDPARSE_CMAKE, NULL, &plan,
                              NULL);
    assert(rc == MULIB_BUILDPARSE_OK || rc == MULIB_BUILDPARSE_INVALID);
    mulib_buildplan_destroy(plan);
    plan = NULL;
  }
}

static int
content_digest(void *user, const char *path, unsigned char digest[32])
{
  (void)user;
  if (!strcmp(path, "missing"))
    return 0;
  assert(mulib_sha256(path, strlen(path), digest));
  return 1;
}

typedef struct probe_context {
  uint64_t input_time;
  int different;
} probe_context_t;

static int
probe_path(void *user, const char *path, mulib_buildstate_path_t *state)
{
  probe_context_t *context = (probe_context_t *)user;
  memset(state, 0, sizeof(*state));
  state->exists = state->regular = state->has_digest = 1;
  state->mtime_ns = !strcmp(path, "input") ? context->input_time : 2;
  memset(state->digest, !strcmp(path, "input") && context->different ? 2 : 1,
         sizeof(state->digest));
  return 0;
}

static void
states(void)
{
  const char *argv[] = {"cc", "-c", "main.c"};
  const char *inputs[] = {"z.h", "main.c"};
  const char *inputs_reordered[] = {"main.c", "z.h"};
  const char *outputs[] = {"main.o"};
  const char *allowed[] = {"PATH", "LANG"};
  mulib_buildstate_env_t env[] = {{"PATH", "/bin"}, {"LANG", "C"}};
  mulib_buildstate_env_t env_reordered[] = {{"LANG", "C"}, {"PATH", "/bin"}};
  mulib_buildstate_action_t a = {
      argv, 3, "build", env, 2, inputs, 2, outputs, 1, 1000, 2};
  mulib_buildstate_action_t b = {
      argv, 3, "build", env_reordered, 2, inputs_reordered, 2,
      outputs, 1, 1000, 2};
  const char *dirty_inputs[] = {"input"};
  const char *dirty_outputs[] = {"output"};
  probe_context_t probe = {1, 0};
  char first[65], second[65], provenance[65], file_hash[65];
  FILE *file;
  int dirty;
  assert(!mulib_buildstate_action_fingerprint(&a, first));
  assert(!mulib_buildstate_action_fingerprint(&b, second));
  assert(!strcmp(first, second));
  assert(mulib_buildstate_hermetic(&a, allowed, 2));
  assert(!mulib_buildstate_provenance_fingerprint(
      &a, content_digest, NULL, provenance));
  assert(strcmp(first, provenance));
  assert(!mulib_buildstate_is_dirty(
      dirty_inputs, 1, dirty_outputs, 1, MULIB_BUILDSTATE_TIMESTAMP,
      probe_path, &probe, &dirty));
  assert(!dirty);
  probe.input_time = 3;
  assert(!mulib_buildstate_is_dirty(
      dirty_inputs, 1, dirty_outputs, 1, MULIB_BUILDSTATE_TIMESTAMP,
      probe_path, &probe, &dirty));
  assert(dirty);
  probe.input_time = 1;
  assert(!mulib_buildstate_is_dirty(
      dirty_inputs, 1, dirty_outputs, 1, MULIB_BUILDSTATE_CONTENT,
      probe_path, &probe, &dirty));
  assert(!dirty);
  probe.different = 1;
  assert(!mulib_buildstate_is_dirty(
      dirty_inputs, 1, dirty_outputs, 1, MULIB_BUILDSTATE_CONTENT,
      probe_path, &probe, &dirty));
  assert(dirty);
  file = fopen("buildstate-input.tmp", "wb");
  assert(file && fwrite("abc", 1, 3, file) == 3 && !fclose(file));
  assert(!mulib_buildstate_file_fingerprint("buildstate-input.tmp", 3,
                                            file_hash));
  assert(!strcmp(file_hash,
                 "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  assert(mulib_buildstate_file_fingerprint("buildstate-input.tmp", 2,
                                           file_hash)
         == MULIB_BUILDSTATE_LIMIT);
  assert(!remove("buildstate-input.tmp"));
}

int
main(void)
{
  plans();
  depfiles();
  parsers();
  states();
  return 0;
}
