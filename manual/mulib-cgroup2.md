# mulib-cgroup2

## Build
`MULIB_IMPL_CGROUP2`, `MULIB_IMPL_SANDBOX_PROFILE`; Linux cgroup v2 filesystem,
`-D_GNU_SOURCE`. No external service or privileged helper.

Initialize with `MULIB_CGROUP2_INIT`. Supply a trusted, opened delegated
cgroup directory with enabled memory, pids and cpu controllers. The kernel
checks filesystem magic and directory type, then makes one exclusive leaf
underneath it. Names are single components (ASCII letters/digits/underscore,
hyphen/dot; no dot/dot-dot). It sets `memory.max` (rounded down to page size),
optional `memory.swap.max=0`, `pids.max`, `cpu.max`, rereading values and
rejecting mismatches. Requires a writable `cgroup.kill`. Kernel and ancestor
limits may impose tighter bounds. It never enables controllers or moves the
supervisor itself. `create` duplicates parent FD and owns both handles. On a
failed partial create it attempts removal; if removal fails it retains a
cleanup handle and returns state error.

- `mulib_cgroup2_verify` rereads requested limits and checks that `cgroup.kill`
  can still be opened for writing; launcher insists on swap=0
  and pids=1 as well as the exact approved CPU rate/memory ceiling.
- `mulib_cgroup2_attach` writes a trusted live child PID to `cgroup.procs`.
  Launcher holds its child behind a gate until this succeeds and separately
  verifies that its child is the sole member. Standalone
  caller must ensure PID identity/liveness, never take a PID from a plugin.
- `mulib_cgroup2_populated` reads `cgroup.events` and returns 0/1 via out.
- `mulib_cgroup2_kill` requests kernel group-wide kill, not necessarily an
  instantaneous reap; poll population and wait for owned child.
- `mulib_cgroup2_destroy` refuses a populated group; removes the owned leaf
  and closes handles on success. Errors retain resources for cleanup.

The enclosing cgroup parent and its rights must stay trusted, and each group
is dedicated to exactly one worker. Do not expose cgroup filesystem access to
plugins. `cpu.max` is a rate, not cumulative CPU time; combine with RLIMIT_CPU
and wall clock deadlines. Memory accounting differs from process RSS; strict
mode also sets RLIMIT_AS. Kernel v2 controller delegation is required; read-only
mounts and unavailable controllers must fail closed.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-cgroup2.h`](../kernels/mulib-cgroup2.h) |
| Implementation switch | `MULIB_IMPL_CGROUP2` |
| Repository-local includes | [mulib-sandbox-profile](mulib-sandbox-profile.md) |
| Complete example | [`examples/modules/sandbox-process.c`](../examples/modules/sandbox-process.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/modules/sandbox-process.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
