# MuLIB sandbox kernels

Eight C11 single-header modules for a broker-only Linux plugin worker runtime.
They are intended for upstream MuLIB (`include/`) and subsequently Shylock's
separate plugin control plane. No third-party library is required. These are
implementations, not stub declarations. Linux-specific operations return an
explicit unsupported/error status when unavailable. No isolation feature is
silently downgraded.

## Contents

| Kernel | Functionality |
|---|---|
| `mulib-sandbox-profile.h` | Versioned strict policy, limits, masks, validation and status codes |
| `mulib-seccomp.h` | Architecture-checked classic BPF compiler, masked syscall argument rules, worker allowlist and TSYNC installation |
| `mulib-sandbox-process.h` | Descriptor-based trusted ELF launch, sanitized environment/FDs, UID/GID drop, sandbox bootstrap, handshake and real security status checks, deadline and reap |
| `mulib-landlock.h` | Kernel ABI discovery and fd-based filesystem allowlists with required minimum ABI |
| `mulib-namespace.h` | User mappings; mount, network, IPC, UTS and future-child PID namespaces |
| `mulib-cgroup2.h` | Exclusive leaf creation under an authorized delegated parent, verified limits, attach, status, group kill and cleanup |
| `mulib-ipc-framed.h` | Binary-safe capped stream frames with absolute monotonic deadlines, partial I/O and poisoned-state handling |
| `mulib-capbroker.h` | Host-approved grants, request/response quotas, handlers, sticky denials, and commit/abort/audit hooks |

## Integrate

The eight modules are registered in [MANIFEST.yaml](MANIFEST.yaml) and the
[library index](manual/index.md). Headers, manuals, examples and tests are
part of this tree. See the [On1x integration notes](on1x/docs/kernel-sandbox.md)
before exposing these APIs through Exotract or ExoML.

Set `_GNU_SOURCE` before any system header (`-D_GNU_SOURCE`) on Linux. The
header-only implementation pattern is unchanged: enable each required
`MULIB_IMPL_*` switch exactly once in one implementation translation unit;
other TUs include declarations only. The full implementation set is shown in
`tests/sandbox-implementation.c`. C11 + Linux/POSIX system interfaces do not require
libseccomp, libcap, libcontainer, systemd, or another external library. Native
seccomp/launcher support is explicitly limited to little-endian Linux LP64
x86-64 and AArch64; only x86-64 was runtime-tested here. POSIX framing targets
connected nonblocking-capable stream sockets with a SIGPIPE-safe send path.

<a id="extensive-examples-and-test-coverage"></a>
## Test and try

```sh
CC=gcc BUILD_DIR=build/sandbox-gcc ./tests/run.sh
CC=clang BUILD_DIR=build/sandbox-clang ./tests/run.sh

# Worker executable is also its own trusted bootstrap.
cc -D_GNU_SOURCE -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
   examples/modules/sandbox-process.c -o /tmp/mulib-sandbox-demo
/tmp/mulib-sandbox-demo

cc -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude \
   examples/modules/capbroker.c -o /tmp/mulib-capbroker-demo
/tmp/mulib-capbroker-demo
```

On a host with writable, delegated cgroup v2 memory/pids/cpu controllers and
`cgroup.kill` (not the system root), run:

```sh
MULIB_TEST_CGROUP_PARENT=/path/to/delegated/cgroup ./tests/run_sandbox.sh
```

This performs real creation, readback, attach, population, kill and removal
under that parent. Run only against a trusted, test-owned delegation. Without
one, the positive cgroup tests report a skip. Landlock and namespace tests
probe the actual kernel and report feature unavailability. See the
[sandbox verification record](manual/sandbox-verification.md) for results and
untested configurations, including AArch64.

## Threat model and important limitations

**The strict worker launcher requires a trusted bootstrap executable.** It is
not a safe way to execute arbitrary untrusted ELF files. The supervisor pins a
readable ELF descriptor, removes inherited environment and unrelated FDs,
optionally applies pre-exec namespace/Landlock/cgroup restrictions, and execs the
approved ELF. The dynamic loader, static constructors, and startup code run
before the strict seccomp filter: they must be trusted. The bootstrap calls
`mulib_sandbox_worker_enter` before parsing or executing untrusted data/code,
then waits for the first request. The parent independently checks `/proc`
seccomp filter count, mode, no-new-privileges and capability sets and validates
its ready message. These checks are not remote attestation of a filter's exact
contents. Never launch an ELF controlled by a plugin author under that trust
assumption. Store the executable and dependencies in trusted immutable paths;
verify identity/content hashes in the supervisor's installation policy. The fd
pins the inode, not its mutable contents or its linked libraries.

For Shylock, compile an approved On1x worker with a trusted C bootstrap or run
untrusted interpreted code behind such a bootstrap. A fully arbitrary native
executable with untrusted constructors requires stronger pre-exec confinement
(e.g. an independently reviewed native container backend or VM; not supplied
by this bundle). Asset acquisition/LLM calls must remain brokered; no raw
network, arbitrary file paths, DB descriptors, credentials or dynamic loading
are granted to strict workers. Process creation and `exec` are denied after the
filter is installed. The backing Linux kernel and trusted supervisor/worker
startup are in the TCB; OS isolation is defense in depth, not certification.

- A user namespace is not itself a filesystem sandbox. The mount namespace
  helper makes propagation private; it does not build/pivot a minimal rootfs.
  The PID namespace primitive changes the namespace of **future children**;
  this single-child launcher rejects PID namespace requests instead of claiming
  the worker is isolated in PID space. A future reaper/init backend can compose
  that primitive. The baseline denies new child processes regardless.
- Landlock handles the filesystem rights known through ABI 5; it is not a
  general path metadata or network filter, cannot revoke inherited handles,
  and is unavailable on some kernels. Before exec, allow the trusted ELF and
  its required loader/library reads/exec rights explicitly, preferably using
  a minimal reviewed image. No fallback to weaker ABI when a higher ABI is
  required.
- `RLIMIT_AS` limits virtual address space, not actual RSS. The optional cgroup
  policy limits memory accounting, swap, rate, and PIDs; it requires an actual
  delegated cgroup. No automatic delegation or privilege escalation. If a
  cgroup is required and cannot be prepared, launch fails.
- Caller drives `mulib_sandbox_process_tick` and terminates expired/poisoned
  workers. There is no hidden watchdog thread. Wall time starts on accepted
  startup; startup has a separate deadline. CPU seconds are cumulative.
- A strict worker is single-threaded (no clone/fork). Supervisor must likewise
  be single-threaded for this launcher and exclusively own child reaping, with
  default SIGCHLD handling. Do not use it after starting GUI/runtime threads.
  Use a dedicated single-threaded supervisor process.
- Capability dispatch is transport/serialization independent. The host must
  parse/validate request messages, authenticate identity using the connection,
  attach a trusted subject, and implement authorization for individual data
  objects. The broker is not a JSON parser, TLS client, or database transaction
  implementation. Transaction hooks are explicit and commit-uncertain status
  is preserved. Network/model calls are not rollback-able by these hooks.

## Suggested Shylock architecture

```text
shylock-engine ──────────────────────────────── shylock-data (sole writer)
                                                      ^
shylock-plugind ─── capability/operation broker ──────┘
       |              |            |
       |              |            └─ trusted HTTPS/LLM acquisition service
       |              └─ host-staged mutations / version checks
       +── private inherited socket ── strict Dragnet worker
       +── private inherited socket ── strict Poindexter worker
```

Restrict the data plane to typed operations; never expose generic arbitrary
syscall dispatch. Dragnet proposes evidence; Poindexter requests authorized
review revisions; data service commits. Keep Mune and browser accounting
separate from OS enforcement. Add ExoLib build profiles and independently
reviewed ExoML ownership/ABI/effect contracts before enabling On1x safe imports.
The included headers implement the mechanisms, not a JSON-RPC protocol or
Shylock data-plane services. See the
[independent Shylock worker follow-up](on1x/docs/shylock-worker-followup.md).

## Catalogue-driven design (CDD)

Eight portable C11 single-header modules describe and compose capabilities
without external libraries:

- [Catalogue](manual/mulib-cdd-catalogue.md) and
  [registry](manual/mulib-cdd-registry.md): validate versioned descriptors and
  keep bounded views of borrowed entries.
- [Query](manual/mulib-cdd-query.md) and
  [schema](manual/mulib-cdd-schema.md): filter and rank entries by capability,
  metadata and cost, and check category-specific metadata rules.
- [Plan](manual/mulib-cdd-plan.md) and
  [resolver](manual/mulib-cdd-resolve.md): independently validate sequential
  plans and search for a minimum-cost plan within an explicit state budget.
  Hitting the budget does not certify optimality.
- [Execute](manual/mulib-cdd-execute.md) and
  [diff](manual/mulib-cdd-diff.md): dispatch a validated plan through host-approved
  callbacks with optional compensation, and compare catalogues by identity.

Descriptors and their nested data are borrowed and must remain immutable while
in use. Registry mutation invalidates views and plan indices. CDD does not load
or invoke arbitrary symbols; the host owns dispatch and any needed isolation.
Define the required `MULIB_IMPL_CDD_*` switches exactly once in one C
translation unit, as in the [pipeline example](examples/cdd-pipeline.c).

The root `tests/run.sh` runs the [CDD semantic suite](tests/cdd-test.c) and
the pipeline example via CTest. To run just those targets:

```sh
cmake -S . -B tests/build/cdd -DMULIB_BUILD_SHIRAZ=OFF
cmake --build tests/build/cdd --target mulib_test_cdd mulib_example_cdd_pipeline
ctest --test-dir tests/build/cdd -R '^mulib_(test_cdd|example_cdd_pipeline)$' --output-on-failure
```

Use `sh tests/run-cdd.sh` for the standalone suite, including delayed
implementation switches and C++17 linkage.

## Linux notebook desktop overlay

The [notebook desktop guide](manual/notebook-desktop.md) describes the 83
bounded header-only C11 modules, integration limits, and the strict GCC/Clang
`make check-notebook-desktop` gate. `make check-graphics` still tests the
original graphics overlay separately.
