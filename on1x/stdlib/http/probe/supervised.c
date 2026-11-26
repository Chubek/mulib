#include "supervised.h"

#include <errno.h>
#include <string.h>

_Static_assert((int)BASIS_HTTP_PROBE_CONTEXT_MAX <= (int)BASIS_DAEMON_MAX_CONTEXT,
               "probe context must fit the daemon envelope");
_Static_assert(BASIS_HTTP_PROBE_CONTEXT_MAX <= BASIS_KRON_MAX_JOB_BYTES,
               "probe context must fit Kron");

typedef struct {
  const basis_http_probe_supervised_host *host;
  basis_http_probe_invocation invocation;
  basis_http_probe_job_result result;
} probe_dispatch;

static int probe_host_ok(const basis_http_probe_supervised_host *host)
{
  return host && host->runners && host->runner_count &&
      host->runner_count <= BASIS_DAEMON_MAX_RUNNERS && host->authorize &&
      basis_daemon_validate_limits(&host->limits) == BASIS_DAEMON_OK;
}

static basis_http_probe_job_result probe_result(basis_http_probe_job_status status)
{
  basis_http_probe_job_result ret = {0};
  ret.status = status;
  ret.daemon_event.exit_code = -1;
  return ret;
}

/* Preflight both schema and trusted runner binding before consulting host
 * policy; the public daemon API independently checks again before launch. */
static int probe_authorize(probe_dispatch *dispatch, const unsigned char *data, size_t size)
{
  const basis_http_probe_supervised_host *host = dispatch->host;
  if (basis_http_probe_context_decode(data, size, &dispatch->invocation.keys) !=
      BASIS_HTTP_PROBE_CONTEXT_OK) {
    dispatch->result.status = BASIS_HTTP_PROBE_JOB_INVALID;
    return 0;
  }
  const basis_daemon_job job = { dispatch->invocation.keys.runner, data, size };
  dispatch->result.daemon_status = basis_daemon_validate_job(host->runners, host->runner_count, &job);
  if (dispatch->result.daemon_status != BASIS_DAEMON_OK) {
    dispatch->result.status = BASIS_HTTP_PROBE_JOB_DAEMON;
    return 0;
  }
  if (host->authorize(host->user, &dispatch->invocation) != 1) {
    dispatch->result.status = BASIS_HTTP_PROBE_JOB_DENIED;
    return 0;
  }
  return 1;
}

static int probe_launch(probe_dispatch *dispatch, const unsigned char *data, size_t size)
{
  const basis_http_probe_supervised_host *host = dispatch->host;
  const basis_daemon_job job = { dispatch->invocation.keys.runner, data, size };
  basis_http_probe_job_result *result = &dispatch->result;
  result->has_daemon_event = 1;
  result->daemon_status = basis_daemon_run(host->supervisor, host->runners, host->runner_count,
                                           &job, &host->limits, &result->daemon_event);
  if (result->daemon_status != BASIS_DAEMON_OK)
    result->status = BASIS_HTTP_PROBE_JOB_DAEMON;
  else if (result->daemon_event.state != BASIS_DAEMON_EXITED ||
           result->daemon_event.child_owned || result->daemon_event.exit_code != 0)
    result->status = BASIS_HTTP_PROBE_JOB_EXIT;
  else
    result->status = BASIS_HTTP_PROBE_JOB_OK;
  return result->status;
}

basis_http_probe_job_status
basis_http_probe_run_supervised(const basis_http_probe_supervised_host *host,
                                const basis_http_probe_context *context,
                                basis_http_probe_job_result *out)
{
  if (!out) return BASIS_HTTP_PROBE_JOB_INVALID;
  probe_dispatch dispatch = {0};
  dispatch.host = host;
  dispatch.result = probe_result(BASIS_HTTP_PROBE_JOB_INVALID);
  unsigned char data[BASIS_HTTP_PROBE_CONTEXT_MAX];
  size_t size;
  if (probe_host_ok(host) && host->supervisor &&
      basis_http_probe_context_encode(context, data, sizeof data, &size) ==
          BASIS_HTTP_PROBE_CONTEXT_OK &&
      probe_authorize(&dispatch, data, size))
    (void)probe_launch(&dispatch, data, size);
  *out = dispatch.result;
  return out->status;
}

basis_http_probe_job_status
basis_http_probe_daemonize(const basis_http_probe_supervised_host *host,
                          const basis_http_probe_context *context,
                          basis_daemon_supervisor **handle,
                          basis_http_probe_job_result *out)
{
  if (!out) return BASIS_HTTP_PROBE_JOB_INVALID;
  probe_dispatch dispatch = {0};
  dispatch.host = host;
  dispatch.result = probe_result(BASIS_HTTP_PROBE_JOB_INVALID);
  unsigned char data[BASIS_HTTP_PROBE_CONTEXT_MAX];
  size_t size;
  if (probe_host_ok(host) && !host->supervisor && handle && !*handle &&
      basis_http_probe_context_encode(context, data, sizeof data, &size) ==
          BASIS_HTTP_PROBE_CONTEXT_OK &&
      probe_authorize(&dispatch, data, size)) {
    basis_daemon_supervisor *owner = NULL;
    basis_http_probe_job_result *result = &dispatch.result;
    result->daemon_status = basis_daemon_create_supervisor(&owner);
    if (result->daemon_status == BASIS_DAEMON_OK) {
      const basis_daemon_job job = { dispatch.invocation.keys.runner, data, size };
      result->has_daemon_event = 1;
      result->daemon_status = basis_daemon_start(owner, host->runners, host->runner_count,
          &job, host->limits.startup_ms, host->limits.reap_ms, &result->daemon_event);
      if (result->daemon_status == BASIS_DAEMON_OK || result->daemon_event.child_owned ||
          basis_daemon_destroy_supervisor(&owner) != BASIS_DAEMON_OK)
        *handle = owner;
    }
    result->status = result->daemon_status == BASIS_DAEMON_OK ? BASIS_HTTP_PROBE_JOB_OK
                                                            : BASIS_HTTP_PROBE_JOB_DAEMON;
  }
  *out = dispatch.result;
  return out->status;
}

basis_http_probe_job_status
basis_http_probe_decode_child(int argc, const char *const *argv,
                              const char *expected_runner, basis_http_probe_context *out)
{
  unsigned char data[BASIS_HTTP_PROBE_CONTEXT_MAX];
  basis_http_probe_context decoded;
  size_t size;
  if (!out || !expected_runner ||
      basis_daemon_decode_context(argc, argv, expected_runner, data, sizeof data, &size) !=
          BASIS_DAEMON_OK ||
      basis_http_probe_context_decode(data, size, &decoded) != BASIS_HTTP_PROBE_CONTEXT_OK ||
      strcmp(decoded.runner, expected_runner))
    return BASIS_HTTP_PROBE_JOB_INVALID;
  *out = decoded;
  return BASIS_HTTP_PROBE_JOB_OK;
}

basis_kron_change
basis_http_probe_schedule(basis_kron_vm *vm, const basis_kron_schedule *schedule,
                          int64_t now, const basis_http_probe_context *context,
                          basis_http_probe_authorize_schedule_fn authorize, void *user,
                          uint64_t *id)
{
  unsigned char data[BASIS_HTTP_PROBE_CONTEXT_MAX];
  basis_http_probe_context decoded;
  size_t size;
  if (!vm || !schedule || !authorize || !id ||
      basis_http_probe_context_encode(context, data, sizeof data, &size) !=
          BASIS_HTTP_PROBE_CONTEXT_OK ||
      basis_http_probe_context_decode(data, size, &decoded) != BASIS_HTTP_PROBE_CONTEXT_OK) {
    errno = EINVAL;
    return BASIS_KRON_ERROR;
  }
  const basis_kron_schedule copy = *schedule;
  if (basis_kron_validate_schedule(&copy, now) < 0) return BASIS_KRON_ERROR;
  if (authorize(user, &decoded, &copy, now) != 1) {
    errno = EACCES;
    return BASIS_KRON_ERROR;
  }
  return basis_kron_delegate(vm, &copy, now, data, size, id);
}

static int probe_kron_authorize(void *user, const basis_kron_occurrence *event)
{
  probe_dispatch *dispatch = user;
  dispatch->invocation.scheduled = 1;
  dispatch->invocation.occurrence_id = event->id;
  dispatch->invocation.deadline = event->deadline;
  return probe_authorize(dispatch, event->data, event->size);
}

static int probe_kron_run(void *user, const basis_kron_occurrence *event)
{
  return probe_launch(user, event->data, event->size);
}

basis_kron_dispatch_status
basis_http_probe_dispatch(basis_kron_vm *vm, int64_t now,
                          const basis_http_probe_supervised_host *host,
                          basis_kron_result *out, basis_http_probe_job_result *attempt)
{
  if (!probe_host_ok(host) || !host->supervisor || !out || !attempt) {
    errno = EINVAL;
    return BASIS_KRON_DISPATCH_ERROR;
  }
  probe_dispatch dispatch = {0};
  dispatch.host = host;
  dispatch.result = probe_result(BASIS_HTTP_PROBE_JOB_NOT_RUN);
  const basis_kron_host kron = { &dispatch, probe_kron_authorize, probe_kron_run };
  basis_kron_dispatch_status status = basis_kron_dispatch(vm, now, &kron, out);
  if (status == BASIS_KRON_CONSUMED || status == BASIS_KRON_CONSUMED_UNCERTAIN)
    *attempt = dispatch.result;
  return status;
}
