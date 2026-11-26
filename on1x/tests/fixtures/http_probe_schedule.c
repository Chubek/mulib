/* Cross-package native host fixture. The scheduled data names a runner;
 * only this process maps the name to the current, explicitly allowed binary. */
#include "http/probe/supervised.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const basis_http_probe_context daily = {"daily", "offline-probe", "fixture-v1"};
static const basis_http_probe_context retired = {"daily", "retired-probe", "fixture-v1"};

/* The test compiles the generated On1x application with -Dmain=probe_main. */
int probe_main(void);

static int matches_daily(const basis_http_probe_context *context)
{
  return !strcmp(context->probe, daily.probe) &&
         !strcmp(context->runner, daily.runner) &&
         !strcmp(context->context, daily.context);
}

static int child(int argc, char **argv)
{
  const char *args[3];
  basis_http_probe_context context;
  if (argc != 3 || getenv("ON1X_PROBE_TEST_PARENT")) return 1;
  for (int i = 0; i < argc; ++i) args[i] = argv[i];
  if (basis_http_probe_decode_child(argc, args, daily.runner, &context) != BASIS_HTTP_PROBE_JOB_OK ||
      !matches_daily(&context))
    return 2;
  return probe_main();
}

struct host_state {
  unsigned grants_checked, schedules_checked;
  int allow;
  uint64_t id;
};

static int authorize_schedule(void *opaque, const basis_http_probe_context *keys,
                              const basis_kron_schedule *schedule, int64_t now)
{
  struct host_state *host = opaque;
  ++host->schedules_checked;
  assert(now == 100);
  if (matches_daily(keys)) return schedule->cadence == BASIS_KRON_EVERY;
  /* This key was registered when scheduled, but its runner is unavailable
   * after the restart. Admission grants no launch or egress authority. */
  return !strcmp(keys->probe, retired.probe) && !strcmp(keys->runner, retired.runner) &&
         !strcmp(keys->context, retired.context) && schedule->cadence == BASIS_KRON_ONCE;
}

static int authorize(void *opaque, const basis_http_probe_invocation *job)
{
  struct host_state *host = opaque;
  ++host->grants_checked;
  assert(matches_daily(&job->keys));
  if (job->scheduled)
    assert(job->occurrence_id == host->id && job->deadline == (host->allow ? 110 : 120));
  else
    assert(job->occurrence_id == 0 && job->deadline == 0);
  return host->allow;
}

int main(int argc, char **argv)
{
  basis_kron_vm *vm = NULL;
  basis_kron_result result;
  basis_http_probe_job_result attempt;
  const basis_kron_schedule recurrence = {BASIS_KRON_EVERY, 10, BASIS_KRON_UNTIL_SIGNAL, 7};
  const basis_kron_schedule once = {BASIS_KRON_ONCE, 105, BASIS_KRON_PERSISTENT, 0};
  struct host_state state = {.allow = 1};
  const basis_http_probe_context wrong = {"daily", "unknown-runner", "fixture-v1"};
  basis_daemon_supervisor *supervisor = NULL;
  uint64_t wrong_id;
  int64_t deadline;

  if (argc > 2) return child(argc, argv);
  if (argc != 2 || argv[0][0] != '/' || argv[1][0] != '/') return 64;
  const basis_daemon_runner runner = {daily.runner, argv[0], argv[1]};
  assert(setenv("ON1X_PROBE_TEST_PARENT", "private", 1) == 0);
  assert(basis_daemon_create_supervisor(&supervisor) == BASIS_DAEMON_OK);
  const basis_http_probe_supervised_host host = {
    supervisor, &runner, 1, {2000, 2000, 100, 2000}, &state, authorize
  };
  assert(basis_http_probe_run_supervised(&host, &daily, &attempt) == BASIS_HTTP_PROBE_JOB_OK);
  assert(attempt.daemon_event.state == BASIS_DAEMON_EXITED && attempt.daemon_event.exit_code == 0 &&
         !attempt.daemon_event.child_owned);
  assert(state.grants_checked == 1);
  basis_http_probe_supervised_host asynchronous = host;
  basis_daemon_supervisor *child_owner = NULL;
  basis_daemon_event child_event;
  asynchronous.supervisor = NULL;
  assert(basis_http_probe_daemonize(&asynchronous, &daily, &child_owner, &attempt) ==
         BASIS_HTTP_PROBE_JOB_OK);
  assert(child_owner && basis_daemon_wait(child_owner, 2000, &child_event) == BASIS_DAEMON_OK);
  assert(child_event.state == BASIS_DAEMON_EXITED && child_event.exit_code == 0 &&
         !child_event.child_owned);
  assert(basis_daemon_destroy_supervisor(&child_owner) == BASIS_DAEMON_OK && !child_owner);
  assert(state.grants_checked == 2);
  const unsigned unscheduled = state.grants_checked;
  assert(basis_kron_open(argv[1], &vm) == BASIS_KRON_COMMITTED);
  wrong_id = 0;
  assert(basis_http_probe_schedule(vm, &once, 100, &wrong, authorize_schedule, &state, &wrong_id) ==
         BASIS_KRON_ERROR && !wrong_id);
  assert(basis_http_probe_schedule(vm, &recurrence, 100, &daily, authorize_schedule, &state, &state.id) ==
         BASIS_KRON_COMMITTED);
  assert(basis_http_probe_schedule(vm, &once, 100, &retired, authorize_schedule, &state, &wrong_id) ==
         BASIS_KRON_COMMITTED);
  assert(state.schedules_checked == 3 && state.grants_checked == unscheduled);
  assert(state.id != wrong_id);
  /* Both descriptors outlive the VM, without retaining any authority. */
  basis_kron_close(vm);
  vm = NULL;
  assert(basis_kron_open(argv[1], &vm) == BASIS_KRON_COMMITTED);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_READY && deadline == 105);
  assert(basis_http_probe_dispatch(vm, 104, &host, &result, &attempt) == BASIS_KRON_IDLE);
  assert(state.grants_checked == unscheduled);
  assert(basis_http_probe_dispatch(vm, 105, &host, &result, &attempt) == BASIS_KRON_CONSUMED);
  assert(result.id == wrong_id && result.deadline == 105 && result.disposition == BASIS_KRON_DENIED);
  assert(attempt.status == BASIS_HTTP_PROBE_JOB_DAEMON && !attempt.daemon_event.child_owned);
  assert(state.grants_checked == unscheduled);
  assert(basis_http_probe_dispatch(vm, 110, &host, &result, &attempt) == BASIS_KRON_CONSUMED);
  assert(result.id == state.id && result.deadline == 110 && result.disposition == BASIS_KRON_RAN &&
         result.run_status == 0);
  assert(attempt.status == BASIS_HTTP_PROBE_JOB_OK && attempt.daemon_event.state == BASIS_DAEMON_EXITED &&
         attempt.daemon_event.exit_code == 0 && !attempt.daemon_event.child_owned);
  assert(state.grants_checked == unscheduled + 1);
  state.allow = 0;
  assert(basis_http_probe_dispatch(vm, 120, &host, &result, &attempt) == BASIS_KRON_CONSUMED);
  assert(result.id == state.id && result.deadline == 120 && result.disposition == BASIS_KRON_DENIED);
  assert(attempt.status == BASIS_HTTP_PROBE_JOB_DENIED && !attempt.daemon_event.child_owned);
  assert(state.grants_checked == unscheduled + 2);
  assert(basis_kron_signal(vm, 7) == BASIS_KRON_COMMITTED);
  assert(basis_kron_next(vm, &deadline) == BASIS_KRON_EMPTY);
  assert(basis_http_probe_dispatch(vm, 130, &host, &result, &attempt) == BASIS_KRON_IDLE);
  assert(state.grants_checked == unscheduled + 2);
  basis_kron_close(vm);
  assert(basis_daemon_destroy_supervisor(&supervisor) == BASIS_DAEMON_OK);
  assert(!supervisor);
  puts("durable fixture: one due run, two denials");
  return 0;
}
