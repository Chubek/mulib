#include "runtime.h"
#include "mulib-kronvm.h"

#include <errno.h>
#include <stdlib.h>

struct basis_kron_vm {
  mulib_kronvm_t *kernel;
  int dispatching;
};

_Static_assert(BASIS_KRON_MAX_JOB_BYTES == MULIB_KRONVM_MAX_JOB_BYTES,
               "descriptor bound must agree with KronVM");

static int basis_kron_unavailable(const basis_kron_vm *vm)
{
  if (!vm || vm->dispatching) {
    errno = vm ? EBUSY : EINVAL;
    return 1;
  }
  return 0;
}

basis_kron_change basis_kron_open(const char *directory, basis_kron_vm **out)
{
  if (!directory || directory[0] != '/' || !out) {
    errno = EINVAL;
    return BASIS_KRON_ERROR;
  }
  basis_kron_vm *vm = calloc(1, sizeof(*vm));
  if (!vm) return BASIS_KRON_ERROR;
  int rc = mulib_kronvm_open_jobs(directory, &vm->kernel);
  if (rc == -1) {
    int error = errno;
    free(vm);
    errno = error;
    return BASIS_KRON_ERROR;
  }
  *out = vm;
  return rc ? BASIS_KRON_UNCERTAIN : BASIS_KRON_COMMITTED;
}

void basis_kron_close(basis_kron_vm *vm)
{
  if (!vm || basis_kron_unavailable(vm)) return;
  mulib_kronvm_close(vm->kernel);
  free(vm);
}

static basis_kron_change basis_kron_change_from_kernel(int rc)
{
  if (rc < 0) return BASIS_KRON_ERROR;
  return rc ? BASIS_KRON_UNCERTAIN : BASIS_KRON_COMMITTED;
}

static int basis_kron_convert_schedule(const basis_kron_schedule *schedule,
                                      mulib_kronvm_schedule_t *spec)
{
  if (!schedule) {
    errno = EINVAL;
    return -1;
  }
  switch (schedule->cadence) {
    case BASIS_KRON_ONCE: spec->cadence = MULIB_KRONVM_ONCE; break;
    case BASIS_KRON_EVERY: spec->cadence = MULIB_KRONVM_EVERY; break;
    default: errno = EINVAL; return -1;
  }
  switch (schedule->lifetime) {
    case BASIS_KRON_PERSISTENT: spec->lifetime = MULIB_KRONVM_PERSISTENT; break;
    case BASIS_KRON_FOR: spec->lifetime = MULIB_KRONVM_FOR; break;
    case BASIS_KRON_UNTIL_SIGNAL: spec->lifetime = MULIB_KRONVM_UNTIL_SIGNAL; break;
    default: errno = EINVAL; return -1;
  }
  spec->seconds = schedule->seconds;
  spec->limit = schedule->limit;
  return 0;
}

int basis_kron_validate_schedule(const basis_kron_schedule *schedule, int64_t now)
{
  mulib_kronvm_schedule_t spec;
  if (basis_kron_convert_schedule(schedule, &spec) < 0) return -1;
  return mulib_kronvm_validate_job_schedule(&spec, now);
}

basis_kron_change basis_kron_delegate(basis_kron_vm *vm,
                                     const basis_kron_schedule *schedule,
                                     int64_t now, const void *data,
                                     size_t size, uint64_t *id)
{
  mulib_kronvm_schedule_t spec;
  if (basis_kron_unavailable(vm) || basis_kron_convert_schedule(schedule, &spec) < 0)
    return BASIS_KRON_ERROR;
  return basis_kron_change_from_kernel(
      mulib_kronvm_delegate_job(vm->kernel, &spec, now, data, size, id));
}

basis_kron_change basis_kron_pull(basis_kron_vm *vm, uint64_t id)
{
  if (basis_kron_unavailable(vm)) return BASIS_KRON_ERROR;
  if (!id) {
    errno = EINVAL;
    return BASIS_KRON_ERROR;
  }
  return basis_kron_change_from_kernel(mulib_kronvm_pull(vm->kernel, id));
}

basis_kron_change basis_kron_signal(basis_kron_vm *vm, int signal)
{
  if (basis_kron_unavailable(vm)) return BASIS_KRON_ERROR;
  return basis_kron_change_from_kernel(mulib_kronvm_signal(vm->kernel, signal));
}

basis_kron_next_status basis_kron_next(const basis_kron_vm *vm,
                                     int64_t *deadline)
{
  if (basis_kron_unavailable(vm)) return BASIS_KRON_NEXT_ERROR;
  int rc = mulib_kronvm_next(vm->kernel, deadline);
  if (rc < 0) return BASIS_KRON_NEXT_ERROR;
  return rc ? BASIS_KRON_READY : BASIS_KRON_EMPTY;
}

struct basis_kron_invocation {
  basis_kron_host host;
  basis_kron_result result;
};

static void basis_kron_invoke(void *opaque, uint64_t id, int64_t due,
                              const unsigned char *data, size_t size)
{
  struct basis_kron_invocation *call = opaque;
  const basis_kron_occurrence job = {id, due, data, size};
  if (call->host.authorize(call->host.user, &job) != 1) {
    call->result.disposition = BASIS_KRON_DENIED;
    return;
  }
  call->result.run_status = call->host.run(call->host.user, &job);
  call->result.disposition = call->result.run_status ? BASIS_KRON_RUN_FAILED
                                                     : BASIS_KRON_RAN;
}

basis_kron_dispatch_status basis_kron_dispatch(basis_kron_vm *vm, int64_t now,
                                             const basis_kron_host *host,
                                             basis_kron_result *out)
{
  if (basis_kron_unavailable(vm)) return BASIS_KRON_DISPATCH_ERROR;
  if (!host || !host->authorize || !host->run || !out) {
    errno = EINVAL;
    return BASIS_KRON_DISPATCH_ERROR;
  }
  struct basis_kron_invocation call = {
    .host = *host, .result = { .disposition = BASIS_KRON_EXPIRED }
  };
  vm->dispatching = 1;
  int rc = mulib_kronvm_dispatch_job(vm->kernel, now, basis_kron_invoke, &call,
                                    &call.result.id, &call.result.deadline);
  vm->dispatching = 0;
  if (rc < 0) return BASIS_KRON_DISPATCH_ERROR;
  if (!rc) return BASIS_KRON_IDLE;
  if (rc == 2) call.result.disposition = BASIS_KRON_NOT_RUN_UNCERTAIN;
  *out = call.result;
  return rc == 2 ? BASIS_KRON_CONSUMED_UNCERTAIN : BASIS_KRON_CONSUMED;
}
