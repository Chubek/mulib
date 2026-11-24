# mulib-sandbox-process

## Build
`MULIB_IMPL_SANDBOX_PROCESS` and implementation switches for sandbox-profile,
seccomp, ipc-framed, landlock, namespace and cgroup2. Linux LP64 x86-64/AArch64,
`-D_GNU_SOURCE`, close_range, execveat, seccomp TSYNC and /proc status support.
No external libseccomp or container dependency.

## Contract
This is a **trusted-bootstrap** launcher; not safe for an arbitrary untrusted
native ELF. Loader and constructors execute before strict filtering. A trusted
installed executable must enter restrictions before untrusted code/input.
Supervisor must be single-threaded, with default SIGCHLD disposition and
exclusive waitpid ownership; do not auto-reap or let another process manager
reap this child. Supervisor validates immutable executable identity and library
paths, opens executable readably, and supplies the fd. Launcher validates ELF
magic, regular file, no set-ID or group/world-write bits; executes the pinned
inode with no PATH search, shell or plugin-supplied environment. This check
alone does not establish executable provenance. Host root must select a nonzero
UID and GID; nonroot workers may retain the host identity when policy permits.
The identity-drop path clears supplementary groups; optional USER namespace
maps the identity *after* host drop. A retained identity must have matching
real/effective/saved/filesystem IDs and nonzero effective UID/GID; it retains
its supplementary groups. Separate OS identities are preferable for adversarial
workloads.

## Lifecycle
1. Initialize output with `MULIB_SANDBOX_PROCESS_INIT`. Construct approved
   `mulib_sandbox_launch_t` (profile, executable fd, optional UID/GID drop,
   namespace spec, Landlock rules and dedicated empty cgroup).
2. `mulib_sandbox_process_start` validates each required facility, forks a
   trusted child behind an IPC gate, attaches it to the configured cgroup
   before letting it proceed, applies requested namespaces/Landlock and
   identity policy, sets no_new_privs, cwd=/, stdin/stdout/stderr=/dev/null,
   clears environment (only LC_ALL=C), remaps only setup/broker/exec handles,
   closes all other fds with close_range, and execveat's the trusted ELF.
   Landlock rules must permit the ELF and dynamic loader dependencies before
   exec. Trusted loader code must not access secrets. No automatic fallback.
3. `mulib_sandbox_worker_enter` in the trusted bootstrap reads a private
   same-ABI configuration record from fd 4 and closes fd 4. Drops ambient and
   effective/permitted/inheritable capabilities, disables dumpability and
   sets no_new_privs; sets and reads back hard/soft address space, stack, CPU
   and FD limits, and zero core, file size, memlock and process limits. Rechecks
   expected parent, sets parent-death SIGKILL and rejects `READ_IMPLIES_EXEC`.
   Builds and installs strict seccomp policy before
   reading any plugin request. Any Linux setup failure exits 125.
4. It sends the versioned readiness frame after filter installation. Parent
   verifies shape and separately reads `/proc/<child>/status` to check filter
   mode, exactly one added filter, no_new_privs, cleared capabilities and the
   expected UID/GID (and empty supplementary groups after identity drop).
   Optional pre-exec enforcement follows trusted child setup success; cgroup
   membership is separately read back before releasing the gate and after
   readiness. This does not
   attest exact seccomp bytecode or prohibit a malicious bootstrap from lying;
   bootstrap is a trust requirement. Return 0 only on accepted setup.
5. Parent uses `mulib_sandbox_process_send/receive` on borrowed framing state
   with a single absolute wall deadline. Worker uses `mulib_ipc_*` on supplied
   IPC. There is no internal thread or unlimited blocking wait. The supervisor
   must drive `tick` periodically, which checks the deadline and performs
   bounded kill/reap. Any failed operation after launch requires cleanup and
   staged-effect rollback. Never retry poisoned streams. Parent should close
   its original executable fd after start; optional Landlock rule fds and
   dedicated cgroup are caller-owned. Do not mutate a live group's limits.
6. `poll` is nonblocking, returns AGAIN while child lives, exits close the
   socket and clear pid. `stop` signals the owned unreaped child, uses group
   kill if configured, reaps with a caller-supplied absolute cleanup deadline,
   and verifies group emptiness. On timeout/failure the handle remains where
   applicable for further cleanup. If initial start fails after fork, launcher
   attempts bounded kill/reap; inspect both output `pid` and `group`. With a
   cgroup, call `stop` even after `poll` reaps the child if `group` remains
   set. Do not reuse the process handle until cleanup is finished. Keep the
   cgroup alive until `stop` releases the borrow, then destroy it explicitly.

CPU is cumulative per process. Wall starts at ready (startup bounded
separately); tick must be driven even while idle. A blocking broker service
must obey the same job deadline. Address space is not actual memory; cgroup
controls optional real memory rate/quota. PID namespace option is rejected in
this launcher: the standalone namespace API is a future-child primitive. A
mount namespace alone has no rootfs isolation. Bounding capability set can
remain; no effective/permitted/inheritable/ambient caps and no_new_privs plus
no exec are mandatory. `/proc` verification may be unavailable in restricted
containers; launch fails. Modern OS isolation is no promise against kernel
vulnerabilities or privileged trust-base compromise.

See `examples/modules/sandbox-process.c` and `tests/test_sandbox.c`.

## Verification and limits

`CC=gcc ./tests/run.sh` and `CC=clang ./tests/run.sh` run the C11 fixtures
through CTest. `CC=clang ./tests/run_sandbox.sh` runs just the sandbox fixtures.
For the positive cgroup tests, point `MULIB_TEST_CGROUP_PARENT` at a disposable,
delegated parent with enabled `cpu`, `memory` and `pids` controllers; an explicitly
specified parent that cannot enforce the policy is a test failure, not a skip.
Landlock and namespace positive checks report specific host skips. Sanitizer
mode instruments the supervisor/test runner only, never the strict worker.

See the [sandbox verification record](sandbox-verification.md) for exact host
results and outstanding work, including native AArch64 verification, integrated
launcher PID/rootfs lifecycle, and independent security review.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-sandbox-process.h`](../kernels/mulib-sandbox-process.h) |
| Implementation switch | `MULIB_IMPL_SANDBOX_PROCESS` |
| Repository-local includes | [mulib-cgroup2](mulib-cgroup2.md), [mulib-ipc-framed](mulib-ipc-framed.md), [mulib-landlock](mulib-landlock.md), [mulib-namespace](mulib-namespace.md), [mulib-sandbox-profile](mulib-sandbox-profile.md), [mulib-seccomp](mulib-seccomp.md) |
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
