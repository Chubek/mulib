# mulib-distribcluster

[Manuals](README.md) · [Library index](index.md)

Optional concrete worker executor for `mulib-container-runtime`,
`mulib-ocicontainer` and `mulib-vmm` resources. The required JSON/YAML config
selects nodes and the Mune profile (`library/ldistrib/cluster.lua`) emits tasks
such as `container_task('start','worker-a','start','web')`. Bind `web` on that
worker and set worker options `execute=mulib_distribcluster_execute` and
`user=&registry`. No resource ID is resolved as a host path or shell command.

`mulib_distribcluster_init` initializes an unused caller-owned registry with a
borrowed monotonic clock callback/context. Bind up to 32 unique portable IDs to
prepared container runtimes, OCI records or initialized VMMs. IDs cannot be
reused across kinds. Handles, metadata and backing memory are borrowed; keep
them alive until the registry/worker is no longer used. The registry never
constructs or destroys resources automatically; close is an explicit task.

Task payload is the exact resource ID. Common actions: `status`, `start`, `stop`,
`close`. Unknown resource/kind/action fails (-1); backends propagate errors.
Output is decimal lifecycle state, using the respective library's enum.

* Container actions call the actual supplied runtime backend. start is idempotent
  while RUNNING; pause/resume normalize already desired states. stop sends signal
  15; it does not assert process exit. step submits budget 1 with the local clock,
  treating AGAIN/DONE as a successful poll operation. close is allowed only when
  the backend is inactive and retains metadata after runtime close. All container
  actions require the local clock; the network cannot supply/forge local time.
* OCI actions use the existing **metadata-only** lifecycle model. They do not
  implement isolation, OCI bundles or OS launch. Use a concrete container runtime
  or a custom worker executor when real isolated OCI execution is required.
* VMM actions call start/stop/close/reset/step on an already-provisioned VM and
  its real executor. start/stop normalize RUNNING/READY. step handles one executor
  exit. The registry does not invent RAM, devices, disk images or a hypervisor.

`step` and `reset`, process signals, restarting EXITED containers and service
launch after an exit can have repeatable side effects. Worker caching suppresses
same-attempt duplicates only; **new attempts are not exactly once**. Use attempts: 1
for non-idempotent one-shot operations, or provide a durable/fenced executor.
No live migration, storage replication, network overlay, provisioning scheduler
or distributed lock is claimed. Resource sharing/concurrency is caller-managed.

Define `MULIB_IMPL_DISTRIBCLUSTER` plus all distribcomp switches, and
`MULIB_IMPL_CONTAINER_RUNTIME`, `MULIB_IMPL_CONTAINER`, `MULIB_IMPL_ROOTFS`,
`MULIB_IMPL_VFS`, `MULIB_IMPL_ARCHIVE`, `MULIB_IMPL_BUFFER`,
`MULIB_IMPL_OCICONTAINER`, `MULIB_IMPL_VMM`, `MULIB_IMPL_VMM_MEMORY`,
`MULIB_IMPL_VMMDEV`, `MULIB_IMPL_VIRTIO_OS`, `MULIB_IMPL_VIRTIO`,
`MULIB_IMPL_ENDIAN` once. These are explicit opt-in dependencies, not silently
enabled macros. Linux links math, threads and dl. Runnable example:
`examples/modules/distribcluster.c`; backend execution regressions are in
`tests/test_distribcluster.c`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-distribcluster.h`](../include/mulib-distribcluster.h) |
| Implementation switch | `MULIB_IMPL_DISTRIBCLUSTER` |
| Repository-local includes | [mulib-container-runtime](mulib-container-runtime.md), [mulib-distribcomp](mulib-distribcomp.md), [mulib-ocicontainer](mulib-ocicontainer.md), [mulib-vmm](mulib-vmm.md) |
| Complete example | [`examples/modules/distribcluster.c`](../examples/modules/distribcluster.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/distribcluster.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
