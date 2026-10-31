# mulib-container-runtime

## NAME AND INTEGRATION

Backend-acknowledged lifecycle execution and bounded cooperative polling.
Define `MULIB_IMPL_CONTAINER_RUNTIME` plus `container` and `rootfs` transitive
switches. Example: `examples/modules/container-runtime.c`.

## BACKEND CONTRACT

Operations are copied by value. `prepare(context,spec,rootfs,&handle)` publishes
one owned handle only on success and cleans its own partial resources on failure.
`start` starts/restarts that handle. `poll(handle,budget,&exit_code)` returns
AGAIN or DONE, setting exit_code on DONE. `signal`, `pause` and `destroy` return
zero or negative CT errors. Destroy failure retains ownership for a later retry.
Poll is called in both RUNNING and PAUSED states. A paused backend must report
pending exits while keeping guest execution suspended; otherwise it returns AGAIN.

Prepare/start/poll/destroy are required. Advertised PAUSE/SIGNAL capabilities
require matching callbacks. The backend must explicitly reject resource requests
it cannot honor. Runtime validates specs but does not implement host UID, argv,
filesystem or quota semantics for arbitrary callbacks.

## OPERATIONS

`init` borrows a stable CREATED container and optional rootfs, prepares a backend
handle, and publishes runtime only on success. A requested wall deadline requires
a signal callback. Container/rootfs and callback referents outlive runtime close.

`start` permits CREATED or EXITED and commits RUNNING only after callback success.
`pause(runtime,1,now)` requires RUNNING; zero resumes PAUSED. `signal` accepts
numbers 1–64 in RUNNING/PAUSED and does not commit EXITED. `step` submits a positive
work budget and translates DONE into EXITED, recording caller time and code.
Polling an already-exited container returns DONE without invoking the backend.

A nonzero absolute wall_deadline sends signal 9 once when caller time reaches
it. Subsequent polling confirms exit, including from PAUSED. No implicit clock,
thread or asynchronous timer exists. Work budgets mean whatever the backend
documents: QVM/JIT instructions, or native VM exits.

## CLOSE, ERRORS AND STATISTICS

`close` accepts only inactive CREATED/EXITED records, destroys backend resources,
commits DELETED, then zeroes runtime. Running/paused close returns EBUSY. Closing
a zeroed/already-closed runtime succeeds. Generation exhaustion is checked before
callbacks that would cause a state change. Callback reentry returns EBUSY.
Lifecycle preflight reserves the exit/deletion generations before start or pause,
including restarts and resumes near the counter limit.

polls and budget_submitted saturate; submitted budget is not actual instruction
usage. last_error retains the most recent negative callback result. Positive
callback results outside documented control flow become EIO. Failed callbacks
leave lifecycle metadata unchanged, but application callbacks may have their own
side effects. See [container-stack](container-stack.md) for ownership and tests.
