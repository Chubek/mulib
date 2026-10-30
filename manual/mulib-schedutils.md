# schedutils — native scheduling and deadline arithmetic

Define `MULIB_IMPL_SCHEDUTILS`; no dependencies.
See [`examples/modules/schedutils.c`](../examples/modules/schedutils.c).

`current` queries the calling process's native policy and priority with
`sched_getscheduler` and `sched_getparam`; outputs are committed only when
both succeed. `yield` calls `sched_yield`. These return 0 success, -1 invalid,
-2 native error, -3 unsupported. Native policy numbers are not translated and
no privileged scheduler changes are performed. The platform must expose these
POSIX scheduler calls for the Unix backend.

`next(previous, period, now, &next)` is portable checked arithmetic for periodic
work. All values use caller-chosen monotonic ticks. A future previous deadline
is retained; otherwise the first scheduled tick strictly after now is returned,
skipping missed periods without drift. Period zero and uint64 overflow fail
with unchanged output. Return 1 success or 0 failure. The function never reads
a clock, sleeps, allocates, or changes native process scheduling.

Tests: `tests/test_ipc_extensions.c`, including UINT64_MAX and equality cases.
