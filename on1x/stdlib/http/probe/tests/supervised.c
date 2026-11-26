#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "../supervised.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  int calls, decision, scheduled;
  uint64_t id;
  int64_t deadline;
} policy_state;

static int authorize(void *user, const basis_http_probe_invocation *invocation)
{
  policy_state *state = user;
  assert(strcmp(invocation->keys.probe, "health") == 0);
  assert(strcmp(invocation->keys.runner, "probe-fixture") == 0);
  assert(invocation->scheduled == state->scheduled);
  assert(invocation->occurrence_id == state->id);
  assert(invocation->deadline == state->deadline);
  ++state->calls;
  return state->decision;
}

static int authorize_schedule(void *user, const basis_http_probe_context *context,
    const basis_kron_schedule *schedule, int64_t now)
{
  unsigned *admissions = user;
  assert(schedule->seconds == now + 1);
  assert(strcmp(context->probe, "health") == 0);
  ++*admissions;
  return strcmp(context->context, "blocked") != 0;
}

static int child(int argc, const char *const *argv)
{
  basis_http_probe_context context;
  if (basis_http_probe_decode_child(argc, argv, "probe-fixture", &context) ||
      strcmp(context.probe, "health") != 0) return 99;
  if (strcmp(context.context, "check") == 0) return 0;
  if (strcmp(context.context, "exit7") == 0) return 7;
  if (strcmp(context.context, "linger") == 0) {
    const struct timespec pause = {10, 0};
    (void)nanosleep(&pause, NULL);
    return 96;
  }
  return 98;
}

int main(int argc, char **argv)
{
  if (argc >= 3) {
    /* This descriptor fits in one daemon envelope chunk. */
    if (argc != 3) return 97;
    const char *args[] = { argv[0], argv[1], argv[2] };
    return child(argc, args);
  }
  assert(argc == 2 && argv[1][0] == '/');
  char *self = realpath(argv[0], NULL);
  assert(self);
  const basis_daemon_runner runner = { "probe-fixture", self, argv[1] };
  basis_daemon_supervisor *supervisor = NULL;
  assert(basis_daemon_create_supervisor(&supervisor) == BASIS_DAEMON_OK);
  policy_state state = { .decision = 1 };
  basis_http_probe_supervised_host host = {
    supervisor, &runner, 1, {2000, 2000, 10, 2000}, &state, authorize
  };
  basis_http_probe_context context = { "health", "probe-fixture", "check" };
  basis_http_probe_job_result attempt;
  assert(basis_http_probe_run_supervised(&host, &context, &attempt) == BASIS_HTTP_PROBE_JOB_OK);
  assert(state.calls == 1 && attempt.daemon_status == BASIS_DAEMON_OK && attempt.has_daemon_event);
  assert(attempt.daemon_event.state == BASIS_DAEMON_EXITED && !attempt.daemon_event.child_owned);

  strcpy(context.context, "exit7");
  assert(basis_http_probe_run_supervised(&host, &context, &attempt) == BASIS_HTTP_PROBE_JOB_EXIT);
  assert(state.calls == 2 && attempt.daemon_event.exit_code == 7);
  strcpy(context.context, "check");

  state.decision = 2;  /* Only exact approval counts. */
  assert(basis_http_probe_run_supervised(&host, &context, &attempt) == BASIS_HTTP_PROBE_JOB_DENIED);
  assert(state.calls == 3 && !attempt.has_daemon_event);
  state.decision = 1;

  host.limits.run_ms = 0;
  assert(basis_http_probe_run_supervised(&host, &context, &attempt) == BASIS_HTTP_PROBE_JOB_INVALID);
  host.limits.run_ms = 2000;
  strcpy(context.runner, "not-listed");
  assert(basis_http_probe_run_supervised(&host, &context, &attempt) != BASIS_HTTP_PROBE_JOB_OK);
  assert(state.calls == 3);
  strcpy(context.runner, "probe-fixture");

  basis_kron_vm *vm = NULL;
  basis_kron_schedule once = { BASIS_KRON_ONCE, 100, BASIS_KRON_PERSISTENT, 0 };
  uint64_t id = 0;
  unsigned admissions = 0;
  assert(basis_kron_open(argv[1], &vm) == BASIS_KRON_COMMITTED);
  assert(basis_http_probe_schedule(vm, &once, -1, &context, authorize_schedule, &admissions, &id) ==
         BASIS_KRON_ERROR);
  assert(!admissions && !id);
  strcpy(context.context, "blocked");
  assert(basis_http_probe_schedule(vm, &once, 99, &context, authorize_schedule, &admissions, &id) ==
         BASIS_KRON_ERROR);
  assert(admissions == 1 && !id && errno == EACCES);
  strcpy(context.context, "check");
  assert(basis_http_probe_schedule(vm, &once, 99, &context, authorize_schedule, &admissions, &id) ==
         BASIS_KRON_COMMITTED);
  assert(id != 0 && state.calls == 3 && admissions == 2);
  basis_kron_close(vm);
  vm = NULL;

  assert(basis_kron_open(argv[1], &vm) == BASIS_KRON_COMMITTED);
  basis_kron_result result = { .id = UINT64_MAX };
  attempt.status = BASIS_HTTP_PROBE_JOB_EXIT;
  assert(basis_http_probe_dispatch(vm, 99, &host, &result, &attempt) == BASIS_KRON_IDLE);
  assert(attempt.status == BASIS_HTTP_PROBE_JOB_EXIT && result.id == UINT64_MAX);
  state.scheduled = 1;
  state.id = id;
  state.deadline = 100;
  assert(basis_http_probe_dispatch(vm, 100, &host, &result, &attempt) == BASIS_KRON_CONSUMED);
  assert(state.calls == 4 && result.id == id && result.disposition == BASIS_KRON_RAN);
  assert(attempt.status == BASIS_HTTP_PROBE_JOB_OK && attempt.daemon_event.exit_code == 0);
  assert(basis_http_probe_dispatch(vm, 100, &host, &result, &attempt) == BASIS_KRON_IDLE);
  assert(state.calls == 4);

  /* Same durable descriptor, changed host policy: no stale permission. */
  once.seconds = 101;
  assert(basis_http_probe_schedule(vm, &once, 100, &context, authorize_schedule, &admissions, &id) ==
         BASIS_KRON_COMMITTED);
  state.decision = 0;
  state.id = id;
  state.deadline = 101;
  assert(basis_http_probe_dispatch(vm, 101, &host, &result, &attempt) == BASIS_KRON_CONSUMED);
  assert(state.calls == 5 && result.disposition == BASIS_KRON_DENIED);
  assert(attempt.status == BASIS_HTTP_PROBE_JOB_DENIED);
  assert(basis_http_probe_dispatch(vm, 101, &host, &result, &attempt) == BASIS_KRON_IDLE);

  /* An unrelated descriptor in the same queue is denied before app policy. */
  const unsigned char foreign[] = { 'P', 'R', 'B', 2 };
  once.seconds = 102;
  assert(basis_kron_delegate(vm, &once, 101, foreign, sizeof foreign, &id) == BASIS_KRON_COMMITTED);
  assert(basis_http_probe_dispatch(vm, 102, &host, &result, &attempt) == BASIS_KRON_CONSUMED);
  assert(state.calls == 5 && result.disposition == BASIS_KRON_DENIED);
  assert(attempt.status == BASIS_HTTP_PROBE_JOB_INVALID);
  assert(basis_http_probe_dispatch(vm, 102, &host, &result, &attempt) == BASIS_KRON_IDLE);

  /* Child failure does not requeue a consumed one-shot promise. */
  once.seconds = 103;
  strcpy(context.context, "exit7");
  assert(basis_http_probe_schedule(vm, &once, 102, &context, authorize_schedule, &admissions, &id) ==
         BASIS_KRON_COMMITTED);
  state.decision = 1;
  state.id = id;
  state.deadline = 103;
  assert(basis_http_probe_dispatch(vm, 103, &host, &result, &attempt) == BASIS_KRON_CONSUMED);
  assert(state.calls == 6 && result.disposition == BASIS_KRON_RUN_FAILED);
  assert(result.run_status == BASIS_HTTP_PROBE_JOB_EXIT && attempt.daemon_event.exit_code == 7);
  assert(basis_http_probe_dispatch(vm, 103, &host, &result, &attempt) == BASIS_KRON_IDLE);

  basis_kron_close(vm);
  assert(basis_daemon_destroy_supervisor(&supervisor) == BASIS_DAEMON_OK && !supervisor);
  host.supervisor = NULL; /* Daemonize returns a brand new owner. */
  state.scheduled = 0;
  state.id = 0;
  state.deadline = 0;
  strcpy(context.context, "check");
  assert(basis_http_probe_daemonize(&host, &context, &supervisor, &attempt) == BASIS_HTTP_PROBE_JOB_OK);
  assert(supervisor && attempt.has_daemon_event && state.calls == 7);
  basis_daemon_event event;
  assert(basis_daemon_wait(supervisor, 2000, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_EXITED && event.exit_code == 0);
  assert(basis_daemon_destroy_supervisor(&supervisor) == BASIS_DAEMON_OK && !supervisor);

  strcpy(context.context, "linger");
  assert(basis_http_probe_daemonize(&host, &context, &supervisor, &attempt) == BASIS_HTTP_PROBE_JOB_OK);
  assert(supervisor && attempt.daemon_event.state == BASIS_DAEMON_RUNNING);
  assert(state.calls == 8);
  assert(basis_daemon_poll(supervisor, &event) == BASIS_DAEMON_OK &&
         event.state == BASIS_DAEMON_RUNNING && event.child_owned);
  assert(basis_daemon_stop(supervisor, 0, 2000, &event) == BASIS_DAEMON_OK);
  assert(event.state == BASIS_DAEMON_STOPPED && !event.child_owned);
  assert(basis_daemon_destroy_supervisor(&supervisor) == BASIS_DAEMON_OK && !supervisor);
  free(self);
  return 0;
}
