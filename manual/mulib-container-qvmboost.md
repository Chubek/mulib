# mulib-container-qvmboost

## NAME AND INTEGRATION

Container backend for the portable Quota VM. Define
`MULIB_IMPL_CONTAINER_QVMBOOST` and `containerboost` transitive switches.
`include-mulib-qvm.h` is the implemented inline VM dependency. Example:
`examples/modules/container-qvmboost.c`.

## CONFIGURATION

`init(config,code,count)` borrows a validated-at-prepare program and selects
1 MiB RAM, 16 MiB combined code/RAM maximum and one million lifetime instructions.
Set host callback/context explicitly for application services. Config/code must
remain alive through prepare; each prepared handle owns its copied VM code/RAM.
Host callback/context remain borrowed for runtime lifetime.

`ops()` supplies prepare/start/poll/signal/pause/destroy. `backend` validates code
and constructs a registry descriptor for PORTABLE and QVM architectures,
advertising PAUSE, SIGNAL and BUDGET. Priority/name are caller-defined.

## EXECUTION

Start/restart resets the VM to zeroed registers/RAM and PC zero. Poll executes at
most the requested instruction count and resumes from the previous PC. HALT
returns its low-byte exit code. VM faults become exit code 125; direct Quota VM
use exposes the underlying sticky fault. Signals 9 and 15 cooperatively stop
execution with 128+signal exit status. Other signals return ENOTSUP. Pause keeps
state intact; resume permits subsequent polling.

Nonzero process CPU requests lower the configured lifetime instruction quota.
Nonzero memory requests lower the combined code/RAM budget. A request below the
configured allocation produces ELIMIT. Requests for more than one process or
open-file limits return ENOTSUP. Rootfs, UID/GID, argv/env and cwd are metadata;
host calls provide any corresponding services explicitly.

## OWNERSHIP AND RESULTS

Runtime owns the handle until successful close. Each container gets independent
VM state, even when configurations share code. CT errors cover invalid programs,
allocation, unsupported requests and lifecycle errors. Total code/RAM allocation
does not include the small handle struct or memory owned by host callbacks.

See [inline QVM](include-mulib-qvm.md) for instruction semantics and
[runtime](mulib-container-runtime.md) for deadline and generation behavior.
