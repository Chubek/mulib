# Linux Pod: one scoped, broker-only worker

`std.container.Pod` runs one fixed byte-echo job in a separate Linux process.
There is no executable, path, PID, FD, callback, or long-lived owner in the
source API. `Pod` here means one worker with an optional set of namespaces and
a dedicated cgroup leaf. It does not implement containers in the Docker/OCI
sense, process trees, image handling, root filesystem isolation, network
configuration, or orchestration.

```on1x
import std.container.Pod as Pod;

fn main()
{
  match Pod.basis_sandbox_echo_byte(255u8, 2000) with
  {
    Ok(byte) => assert(byte == 255u8), Err(_) => assert(false)
  };
}
```

The default build links the scalar adapter without authorizing a worker:
calls with valid arguments return `Err(PodNotAuthorized)`. An authorized host
opts in at build time with `on1x-build.py --enable-pod-fixture`. This compiles
the fixed `pod_worker.c` as a separate static ELF and embeds it in the program.
The application copies those bytes to a size-bounded sealed executable memfd
and uses MuLIB's descriptor-based launcher. There is no external worker path,
launcher command, environment selection, or weaker fallback. The build
operator must trust the compiler, headers, source package, and native linkage.
An ordinary On1x application itself is **not** confined by this API.

## Profiles and limits

| Interface / option | Requested enforcement |
| --- | --- |
| `basis_sandbox_echo_byte(byte, wall_ms)` | Strict seccomp allowlist, verified credentials/no-new-privileges, resource limits, descriptor allowlist, startup handshake, bounded framed IPC |
| `basis_pod_echo_byte(byte, wall_ms, namespaces, false)` with `PodNoNamespace` | Same baseline |
| `PodUserNamespace` | Baseline plus a child-only user namespace mapping the unprivileged caller's IDs to numeric zero inside |
| `PodUserNetworkIpcUts` | Baseline plus child-only user, network, IPC and UTS namespaces |
| `cgroup = true` | Requires an authorized delegated parent; creates and verifies a fresh leaf, attaches the **already confined** worker, verifies membership and limits, and only then sends the first application frame |

Namespace creation uses `mulib_namespace_enter` in the trusted child. An
unavailable requested namespace fails before the first application request.
There is no joining existing namespaces; PID, mount, and Landlock options are
not exposed. The sandbox itself forbids file opens, creation of sockets or
processes, and execution of another image after readiness. A user namespace
does not grant host root; the bootstrap drops effective, permitted, inheritable
and ambient capabilities before entering the worker protocol.

The startup budget is 2,000 ms. `wall_ms` is `1..5000` and bounds the
post-handshake exchange and normal reap on a monotonic clock. RLIMIT_AS is
64 MiB, RLIMIT_STACK 8 MiB, and RLIMIT_CPU 1 second. At most one request
and one reply of one byte each are sent; the frame payload limit is eight
bytes so the MuLIB startup handshake fits. No success is returned without a
matching reply, normal zero-status reap and cgroup removal when requested.
Cleanup has a separate bounded budget: failed startup has MuLIB's 3-second
cleanup window; the adapter can then spend up to 2 seconds on owned child
cleanup. These are userspace deadlines, not a hard real-time guarantee that
descheduling or stalled kernel calls cannot extend elapsed time.

The cgroup profile requires `memory.max = 64 MiB`, zero swap, `cpu.max =
25000 100000` (25% of one core), `pids.max = 1` and a compatible cgroup v2
leaf. These limits apply to the application exchange, **not** to pre-ready
bootstrap, which occurs before migration. CPU time and memory limits during
the exchange do not describe memory already charged to another cgroup before
migration. The adapter does not call `cgroup.kill`: only its own exclusively
waited-for child can be signaled. It never attaches the calling process or
another process and never enables controllers or edits a parent directory's
policy. Adopt a dedicated, exclusive subtree: if another actor can change
limits, add processes, or replace the leaf, it is not an eligible delegation.

## Native host requirements

- Little-endian LP64 Linux x86-64 or AArch64 using the MuLIB strict launcher
  and a static C toolchain for the worker. Only x86-64/glibc is exercised here.
  Linux must permit `MFD_EXEC` (Linux 6.3+) and `execveat`; the launcher also
  requires `close_range`, seccomp filters and TSYNC, and a usable procfs with
  `Seccomp_filters` and `/proc/self/task`.
- Non-root, non-set-ID supervisor with no effective or permitted capabilities.
  The native host must be single-threaded, never re-enter the adapter, prevent
  concurrent reapers, and use the default SIGCHLD disposition. Pod is a
  blocking raw stdlib effect, not callable inside On1x task bodies or admitted
  by KernelBind or scalar MIR.
- The namespace profiles additionally require the kernel/host LSM policy to
  permit unprivileged user namespace creation and one-entry UID/GID maps,
  including `/proc/self/setgroups` control; the extended profile also needs
  network, IPC and UTS namespaces. No privilege elevation is requested.
- The cgroup profile additionally requires an existing exclusively delegated
  writable cgroup v2 directory with memory, CPU, and PIDs controllers already
  enabled for children, swap accounting controls, writable `cgroup.kill` as
  required by the existing MuLIB create/verify helper, and permission to
  migrate the worker from its initial cgroup (including common-ancestor
  permissions). A trusted native installation can override
  `int on1x_container_pod_delegated_parent(void)` to return a borrowed,
  already-open directory FD for this operation. The default always denies.
  The hook must not block or re-enter; On1x code cannot supply the FD. The
  separate cgroup test fixture is the only code that reads a test environment
  variable naming an explicitly supplied parent.

`PodInvalid` identifies malformed input; `PodNotAuthorized` identifies missing
host opt-in or delegation; `PodDenied` includes rejected sandbox startup or
namespace setup; `PodUnsupported` and `PodSystem` represent other platform and
OS failures. `PodTimeout`, `PodProtocol`, `PodWorkerFailed`, `PodLimit`, and
`PodState` distinguish the remaining failure classes. The private ABI
`on1x_container_pod_echo_byte(i64, i64, i64, i64) -> i64` accepts scalars only;
on successful completion it returns `0..255`, otherwise a negative status.
`PodCleanup` means the operation failed and the native scope retains any
unreaped child or surviving leaf; a later call attempts that cleanup before
starting new work. Killing the supervisor itself can leave an empty cgroup
leaf; the host must reconcile that case. No resource is returned to On1x.

## Verification

From the repository root (runs are ordinary non-root processes):

```sh
python3 on1x/tests/test_container_pod.py native gcc /tmp/opencode
python3 on1x/tests/test_container_pod.py namespace gcc /tmp/opencode
python3 on1x/tests/test_container_pod.py native clang /tmp/opencode \
  --cflag=-fsanitize=address,undefined --cflag=-fno-sanitize-recover=all
python3 on1x/tests/test_container_pod.py on1x gcc /tmp/opencode --on1xc build/on1x/on1xc
python3 on1x/tests/test_container_pod.py cgroup gcc /tmp/opencode
```

The native gate includes repeated binary-byte exchanges, sealed-image checks,
authorization and validation failures, a worker blocked waiting for a deliberately
dropped request (deadline, SIGKILL and reap), absence of leaked FDs/children,
retention/retry after injected cleanup failure, and thread/reaper rejection.
The namespace gate checks the child namespace identities before and after actual
`unshare` and verifies the supervisor did not move. The cgroup gate exits 77
unless `ON1X_TEST_POD_CGROUP_PARENT` identifies a
preexisting exclusive delegated parent; it checks both normal and deadline
cleanup. The application test exercises generated C11, linkage, typed results,
the authorization-off default, and frontend rejection. Sanitizers instrument
the supervisor only; an instrumented ELF is not compatible with the strict
worker's syscall and memory limits.

On Linux `6.18.52-1-lts` x86-64/glibc, GCC `16.2.1` and Clang `22.1.8`
passed the native and namespace gates as UID/GID 1000; the Clang supervisor
also passed with ASan/UBSan. Generated C11 ran with both compilers. The cgroup
gate was skipped: no exclusive delegated parent was available. Repository base:
`c2cbbf70537d12ba5015485239d227ef66b25cb8`; LibStone checkout:
`3399047f6c6328b8f07788795c610699e66c060a`.
