# initproc

POSIX child-reaping primitive. `reap` waits for one positive child PID or any
child (-1), optionally nonblocking. EINTR retried. Returns 1 reaped (PID and
shell-style exit status outputs), 0 not ready, 2 no matching child, -1 invalid,
-2 OS error, -3 unsupported. Outputs unchanged unless reaped. Signal death maps
to 128+signal; normal termination uses the exit code.

No fork/exec, PID-1 setup, subreaper registration, signal forwarding or handlers.
Caller owns the supervisor loop and its synchronization/signal policy.
Switch: `MULIB_IMPL_INITPROC`. Example: `examples/modules/initproc.c`.
