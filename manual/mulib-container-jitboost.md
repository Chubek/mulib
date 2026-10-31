# mulib-container-jitboost

## NAME AND INTEGRATION

Native MIR container execution with instruction budgets and suspended host calls.
Define `MULIB_IMPL_CONTAINER_JITBOOST`, the `containerboost` closure, and
`MULIB_IMPL_JIT`, `DYNCALL`, `VMWEAVE`, `MIR`, `EGRAPH`, `REWRITE` switches.
See [stack](container-stack.md) for full names. Example:
`examples/modules/container-jitboost.c`; Linux linking uses `-lm -ldl`.

## CONFIGURATION

`init(config,program)` selects 65536 records, one million lifetime instructions,
and local optimization. Supply MIR arguments, host callback and user context.
Prepare compiles/deep-copies the validated MIR, allocates an execution frame and
copies arguments. Program result type must be I64, WORD or BOOL. Program and
argument arrays can be released after successful preparation; host context stays
borrowed.

`backend` creates a PORTABLE/AMD64 descriptor and probes compiled-in native
support during selection. Actual executable-memory creation can still fail.
There is no interpreter fallback. Current native support follows `mulib-jit`:
Linux AMD64 with RW-to-RX memory transitions.

## EXECUTION AND INSPECTION

Start/restart constructs a fresh frame with the same arguments and code. Poll
executes a bounded additional instruction budget, returning AGAIN on budget
exhaustion or host suspension. `resume(runtime,value)` completes a suspended
CALL without calling the host callback again. Invalid resume state/value fails.

`inspect` copies MIR status, actual step count and result through optional
outputs. Completed values use low eight bits as exit code. Lifetime quota
exhaustion produces 124, MIR faults 125, signals 9/15 produce 137/143. Other
signals are unsupported. Pause/resume preserves execution state.

CPU requests lower the configured instruction quota. Memory/open-file requests
and process counts above one return ENOTSUP. MIR record limits bound program
arrays; executable allocations follow JIT's own checked limits. Rootfs and process
identity/argv/env remain metadata unless host callbacks implement services.

## LIFETIME AND PLATFORM

Close releases native code and frame storage. Native-unmap failure retains the
handle for retry. JIT and callbacks execute in the embedding process, with caller
synchronization and trusted host functions. Work budgets do not interrupt a
blocking callback. Tests execute real native code and restart/signal paths when
native execution is supported.
