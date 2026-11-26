#ifndef BASIS_HTTP_PROBE_SUPERVISED_H
#define BASIS_HTTP_PROBE_SUPERVISED_H

#include "context.h"
#include "../../daemon/supervisor.h"
#include "../../kron/runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Explicit trusted C host API, not a foreign ABI for On1x. Invocation names
 * are registry lookup keys only. The deployment must not reassign a key to
 * unrelated behavior while promises referring to it can still be pending. */
typedef struct {
  basis_http_probe_context keys;
  int scheduled;   /* If false, occurrence_id and deadline are zero. */
  uint64_t occurrence_id;
  int64_t deadline;
} basis_http_probe_invocation;

typedef struct {
  basis_daemon_supervisor *supervisor;
  const basis_daemon_runner *runners;
  size_t runner_count;
  basis_daemon_limits limits;
  void *user;
  /* Exact 1 for approval. Borrowed only during a synchronous call; no
   * reentry, mutation of inputs, VM/supervisor access, or retention.
   * Approval applies to this launch only, not to any in-child I/O. */
  int (*authorize)(void *user, const basis_http_probe_invocation *invocation);
} basis_http_probe_supervised_host;

typedef enum {
  BASIS_HTTP_PROBE_JOB_OK = 0,
  BASIS_HTTP_PROBE_JOB_NOT_RUN = -1,
  BASIS_HTTP_PROBE_JOB_INVALID = -2,
  BASIS_HTTP_PROBE_JOB_DENIED = -3,
  BASIS_HTTP_PROBE_JOB_DAEMON = -4,
  BASIS_HTTP_PROBE_JOB_EXIT = -5
} basis_http_probe_job_status;

typedef struct {
  basis_http_probe_job_status status;
  basis_daemon_status daemon_status;
  int has_daemon_event; /* True only after calling daemon start/run. */
  basis_daemon_event daemon_event;
} basis_http_probe_job_result;

/* Exactly one foreground attempt. Return JOB_OK only for a reaped exit 0.
 * The caller retains ownership of the supervisor, including on timeout;
 * inspect daemon_event.child_owned before cleanup/retry. No body or result
 * snapshots cross process boundaries through this API. */
basis_http_probe_job_status
basis_http_probe_run_supervised(const basis_http_probe_supervised_host *host,
                                const basis_http_probe_context *context,
                                basis_http_probe_job_result *out);

/* Start asynchronously. Require host->supervisor == NULL and a NULL handle
 * slot. Transfer ownership on accepted startup, or when cleanup still needs
 * the owner. JOB_OK means accepted startup, not a child exit result. Use
 * daemon poll/wait/stop/destroy on the handle; run_ms is a caller-enforced
 * wait/stop budget for this asynchronous operation. */
basis_http_probe_job_status
basis_http_probe_daemonize(const basis_http_probe_supervised_host *host,
                          const basis_http_probe_context *context,
                          basis_daemon_supervisor **handle,
                          basis_http_probe_job_result *out);

/* Child-side decoding of the daemon envelope and probe schema. The expected
 * runner is a trusted deployment constant, not argv data. This is not auth. */
basis_http_probe_job_status
basis_http_probe_decode_child(int argc, const char *const *argv,
                              const char *expected_runner,
                              basis_http_probe_context *out);

/* Admission validates the schedule and descriptor before this mandatory host
 * gate; exact 1 permits persistence. Resolve and check all registry keys here.
 * No launch or egress grant is authorized; dispatch checks again independently.
 * Callback arguments are borrowed; do not retain or mutate them or reenter VM. */
typedef int (*basis_http_probe_authorize_schedule_fn)(void *user,
    const basis_http_probe_context *context, const basis_kron_schedule *schedule, int64_t now);

basis_kron_change
basis_http_probe_schedule(basis_kron_vm *vm, const basis_kron_schedule *schedule,
                          int64_t now, const basis_http_probe_context *context,
                          basis_http_probe_authorize_schedule_fn authorize, void *user,
                          uint64_t *id);

/* On IDLE or dispatch error both outputs remain unchanged. For an expired or
 * durably uncertain consumption, attempt->status is JOB_NOT_RUN. If the
 * occurrence was consumed but the runner failed, inspect both outputs; no
 * automatic retry or compensation is performed. */
basis_kron_dispatch_status
basis_http_probe_dispatch(basis_kron_vm *vm, int64_t now,
                          const basis_http_probe_supervised_host *host,
                          basis_kron_result *out, basis_http_probe_job_result *attempt);

#ifdef __cplusplus
}
#endif
#endif
