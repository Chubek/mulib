# `std.kron`: persistent, caller-clocked scheduling

`std.kron` is independent of HTTP, Elvi, and Daemon. Use the On1x package
for nominal schedule values and pure validation. The separate trusted C11
companion in [`runtime.h`](runtime.h) owns the live VM, cache and job
callbacks. It uses the actual KronVM queue and KVDB snapshot transactions.
There is no raw On1x `extern`, integer-encoded handle, automatic host setup,
or reviewed foreign ABI for native VM ownership or callbacks.

```on1x
from std.kron import KronTime, KronDuration, basis_kron_every,
  basis_kron_persistent, basis_kron_first_deadline;

fn main() -> i64 {
  let now = record<KronTime>{seconds: 100};
  let period = record<KronDuration>{seconds: 5};
  let schedule = basis_kron_every(period, basis_kron_persistent());
  return match basis_kron_first_deadline(schedule, now) with {
    Ok(first) => {
      if first["seconds"] != 105 { return 1; }
      0
    },
    _ => 1
  };
}
```

The package uses canonical `Option` for the time, duration, logical signal
and promise-ID constructors and `Result` for preflighting a schedule.
Preflight rechecks values created with record literals. It grants no authority
and performs no I/O. Use the native companion for execution:

| Operation | Native entry point | Behavior |
|---|---|---|
| Open / close | `basis_kron_open`, `basis_kron_close` | Hold an exclusive cache lock until close. Closing leaves work pending. |
| Check a schedule | `basis_kron_validate_schedule` | Validate types/ranges against a supplied clock without allocating or touching the cache. |
| Delegate | `basis_kron_delegate` | Copy the descriptor and persist its plan. Return a stable, positive `uint64_t` ID. |
| Pull | `basis_kron_pull` | Cancel by ID; unknown positive IDs return `ENOENT`. |
| Signal | `basis_kron_signal` | Atomically cancel all matching logical signals in `1..INT32_MAX`. Not a Unix signal handler API. |
| Next deadline | `basis_kron_next` | Distinguish empty, ready and error, without consuming. |
| Dispatch | `basis_kron_dispatch` | Consume at most one due occurrence and report its ID, due time and disposition. |

All clock values are nonnegative Unix seconds, supplied by the host. `ONCE`
takes an absolute deadline, which may be in the past. `EVERY` takes a positive
interval; the first deadline is admission time plus that interval. Late
dispatch coalesces missed ticks and keeps the original cadence. `FOR` is
relative to admission, with an inclusive expiry. A first deadline after
expiry is invalid; if dispatch time is after expiry the occurrence is retired
without running. Recurrence stops rather than wrapping at `INT64_MAX`.
Equal deadlines dispatch in enqueue order, including across reopens.

## Host trust boundary

The host must provision a dedicated, trusted absolute directory before open
and ensure its directory entry is durable if required. Provision it as
`0700`. The final directory and any existing `events.kvdb` must be owned by
the effective user, have no group/other access bits, and not be symlinks.
The snapshot must be a regular file. These checks run before KVDB reads, not
as a substitute for a trusted path: KVDB opens by path. Keep the directory,
its path and parent directories stable and trusted; the checks do not
protect against another process with authority to replace names or change
permissions. The scheduler does not consult a default path, clock, registry
or process environment. Store each application's descriptors in its own
cache; no other code may alter the path or data behind a live VM. Serialize
calls on a VM and close it exactly once.
Do not use an inherited VM in a child after `fork`; its lock descriptor is
closed on `exec`.

An application descriptor is `1..65536` uninterpreted bytes, including
embedded zero bytes. The scheduler checks its record structure and byte
length, not the application's schema. Use versioned data or registry keys;
do not put pointers, program text, executable paths, secrets or grants in the
descriptor. The companion does not interpret KronQL, launch children, or
provide an exec/shell callback. Its cache profile cannot be opened by the
legacy KronQL runner or vice versa. The old KronVM entry points, cache format
and shell behavior are unchanged.

Both native host callbacks must be provided for each dispatch. The first
must decode the descriptor, reject unknown jobs, and check current policy.
Only an exact return of `1` permits the invocation of `run`; denial consumes
the occurrence without calling `run`, even if the job was authorized earlier.
No authorization is persisted. `run` resolves the validated job with
trusted application code and returns zero for success or an application status
for failure. The callbacks borrow the occurrence and bytes only for each
synchronous call; neither may keep them or reenter or close the VM. The
application owns policy changes, external effects and result reporting.

## Commit and crash semantics

`COMMITTED` and `UNCERTAIN` both report visible state changes. `UNCERTAIN`
means the snapshot replacement was visible but directory sync failed. An
uncertain `open` still transfers a live VM, and uncertain admission still
writes an ID. A pre-commit error leaves the previous queue and output values
untouched. Do not blindly retry a visible but uncertain operation.

Dispatch commits retirement or advancement before calling the host. It
reports `CONSUMED_UNCERTAIN` and skips **both** callbacks when durability
cannot be confirmed. It also skips them on expiry, idle, or pre-commit
failure. A crash after commit and before the callback may skip an occurrence;
a failed or denied run is not retryable by dispatch at the old deadline. An
uncertain commit may roll back on crash. This is not an exactly-once guarantee
for host effects. For recurring jobs, the next cadence point remains queued
after denial, run failure or uncertain consumption unless the plan has ended.
Mutation writes a snapshot; budget time and memory for all pending descriptor
data, not just the per-job limit.

## Build and check

On POSIX with `flock` and atomic replacement support, build/link
`runtime.c` with the standalone `kernels.c` and `-I kernels`. In a composed
application, omit `kernels.c` if a unified implementation TU already
implements KronVM, Evtsched, KVDB, Date, Endian, Buffer, Varint and CRC32.
The On1x-only package does not need a native link plan.

Repository-root examples (choose a writable output directory):

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG \
  -Ikernels -Ion1x/stdlib/kron on1x/stdlib/kron/tests/test_kron.c \
  on1x/stdlib/kron/runtime.c on1x/stdlib/kron/kernels.c -o /tmp/test_std_kron
/tmp/test_std_kron
on1xc --emit-c --stdlib on1x/stdlib on1x/stdlib/kron/tests/schedule.on1x \
  -o /tmp/std_kron_schedule.c
cc -std=c11 -Ion1x/runtime /tmp/std_kron_schedule.c \
  on1x/runtime/on1x_runtime.c -pthread -o /tmp/std_kron_schedule
/tmp/std_kron_schedule
```

With a GNU-compatible linker, also test pre-/post-replacement failures and
allocation failures using `-DBASIS_KRON_TEST_WRAP` and
`-Wl,--wrap=fsync,--wrap=malloc,--wrap=calloc,--wrap=realloc` in the native
test build.
