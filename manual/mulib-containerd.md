# mulib-containerd

## NAME AND INTEGRATION

Embeddable container daemon composing execution pools, backend registry, image
cache, filesystem exports and logs. Define `MULIB_IMPL_CONTAINERD` and closures
for `containterpool`, `containerd-cache`, `containerd-mount`, `container-log` and
`conainerd-ipc`. Complete explicit switches and runnable commands are in
[container-stack](container-stack.md). Example: `examples/modules/containerd.c`.

## CONFIGURATION

`default_options` selects 64 containers, 16 backends, 32 cached images/128 MiB,
1024 log records/1 MiB with 4096-byte messages, 16 bindings per export, 1 MiB FUSE
transfers and default image import quotas. `init` initializes every subsystem
atomically. It starts no listener, process, guest or thread. Keep daemon at a
stable address and externally serialize operations.

Register adapters into `daemon.backends`. `create(image,length,id,backend,caps,
now)` imports/prepares a container and exports its rootfs, without starting it.
Duplicate identities fail. `create_cached` pins a validated cached image around
the same operation and releases the cache lease on every path. Creation failure
rolls back owned metadata; external-backend cleanup failures follow their resource
retention contract and can require later explicit cleanup.

## COMMANDS

`dispatch(command,&status)` validates opcode/ID/value and optionally compares
expected_generation against the current container generation. Zero disables this
check. Caller-provided command.now must be trusted monotonic time; the control
bridge substitutes server time for remote input.

START/PAUSE/RESUME/STOP/STEP delegate to runtime. STOP delivers a signal; STEP
confirms completion. STATUS returns metadata without mutation. DELETE requires
inactive execution, no pool references and no mount leases, then closes runtime,
unexports root and removes the pool entry. Its success response is a final DELETED
snapshot. Deletion does not leave a persistent tombstone in the daemon table.

The output status changes only on success. Lifecycle events enter the bounded log;
log allocation/quota failures increment log_failures without undoing an executed
operation. commands/failures provide operational counters, not a durable audit.

## PROGRESS AND SHUTDOWN

`tick(visits,budget,now,report)` advances round-robin pool polling and sweeps
expired unpinned cache entries. `free(now)` rejects active/borrowed resources,
progressively closes inactive entries, then frees exports/cache/registry/log.
Backend close failure leaves remaining resources live for retry. Callers stop and
poll containers to EXITED before shutdown; free does not silently kill them.
Exports are removed before their roots are freed. If a backend callback acquires
a mount or cache lease after initial preflight, shutdown returns EBUSY and retains
the borrowed resources; release the lease and retry. Already-completed cleanup
remains committed. DELETE likewise retains an exported root if unexporting fails.

Subsystems are exposed for explicit policy/configuration. Avoid directly removing
pool entries while their roots are exported; use daemon DELETE. Destroy bridges
and release cache/mount/pool leases before daemon shutdown. CT errors propagate;
no Docker/containerd socket protocol, host mounts or automatic privilege changes
are implied by this embeddable core.
