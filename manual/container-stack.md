# Container stack

MuLIB's container stack connects owned process descriptions, bounded in-memory
filesystems, deterministic image builds, execution backends, packet endpoints,
and an embeddable daemon. Every operation is explicit and synchronous; applications
choose their event loop, transports, clocks and synchronization.

## Build and run

```sh
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -Iinclude \
  examples/modules/containerd.c -lm -o containerd-example
./containerd-example
python3 tools/check_container_stack.py
CC=clang CXX=clang++ python3 tools/check_container_stack.py --sanitizers
```

Each `examples/modules/<module>.c` is runnable independently. Portable examples
include `examples/container-stack-common.h`, which explicitly lists the complete
portable stack's implementation switches. Native examples additionally use
`examples/container-native-common.h`; link `-lm -ldl -pthread` on Linux.
These are example implementation-TU helpers, not public headers to distribute.

Applications define `MULIB_IMPL_<MODULE>` in exactly one C11 translation unit,
with hyphens converted to underscores: `container-runtime` becomes
`MULIB_IMPL_CONTAINER_RUNTIME`. Include dependency headers normally and define
their switches explicitly, recursively. Never set switches globally for every TU.
The individual manuals list direct dependencies; this table gives the main chains:

| Module family | Supporting implementation modules |
| --- | --- |
| container | Inline container-core dependency |
| rootfs | vfs, archive, buffer |
| rootfs-buildspec | rootfs |
| imgformat | container, crc32 |
| imgbuildspec | imgformat, rootfs-buildspec |
| imgbuilder | imgbuildspec, rootfsbuilder |
| image2container | imgformat, rootfs |
| container-runtime | container, rootfs |
| containerboost | container-runtime, imgformat |
| container-qvmboost | containerboost, inline QVM dependency |
| container-jitboost | containerboost, jit, dyncall, vmweave, mir, egraph, rewrite |
| container-kvmboost | containerboost, kvmcontainer, kvm |
| container-oixboost | containerboost, oix and its documented compiler dependencies |
| container-ipc | crc32 |
| conainerd-ipc | container-ipc, container |
| container-network | container-netdev, packetrules, packetfiter |
| container-vfs | rootfs |
| containerd-mount | container-vfs, fuse |
| containerd-cache | imgformat |
| containterpool | image2container, containerboost |
| containerd | containterpool, containerd-cache, containerd-mount, container-log, conainerd-ipc |
| containerd-accessstub | conainerd-ipc |
| containerd-ctrlbridge | containerd, containerd-accessstub |
| container-stub | conainerd-ipc |
| container-packelf | imgformat, elf, endian |
| container-packzstd | imgformat, inline Zstandard dependency |

OIX's complete closure additionally defines `MULIB_IMPL_PARSER`, alongside
`OIX`, `BASE64`, `CONFUSE`, `EMITTER`, `JSON`, `TOML`, `YAML`, `PARSEC`, `PEG`,
`AST`, `STRBUF`, `UTF8`, `GLOB`, `KVM`, and `CRC32` switches. See the native helper
for a compilable complete set. No public header enables another implementation.

## Ownership and errors

Initialize owning objects before use. Constructors accepting an uninitialized
output publish only on success; they do not release a previous live object.
Blob output APIs instead require `mulib_ct_blob out = {0}` and replace its owned
bytes atomically. Do not shallow-copy live owners. Free in reverse borrowing order:
clients/bridges, leases/exports, runtimes, container/rootfs records, backend contexts.

Common container errors are `MULIB_CT_*`: zero success, `AGAIN=1`, `DONE=2`, and
negative errors. `mulib_ct_strerror` returns a static explanation. Rootfs,
container-VFS and FUSE operations use `MULIB_VFS_*`; archive imports can also
propagate archive errors. Image/build wrappers translate these and expose a
native `detail` where documented. Do not compare numeric codes across families.

Quotas are explicit, with checked arithmetic before allocation. Zero means zero
capacity except process resource requests, where zero means unspecified, and
cache TTL, where zero means no expiry. Core-owned allocations can be injected
using `MULIB_CT_MALLOC`, `MULIB_CT_REALLOC`, `MULIB_CT_FREE`; existing VFS, archive,
OIX and JIT dependencies retain their documented allocators.

## Execution model

Lifecycle edges are CREATED → RUNNING ⇄ PAUSED → EXITED, with restart from EXITED
and deletion from CREATED/EXITED. Backends acknowledge operations before state
changes. A successful signal does not mean exit; polling confirms completion.
Generation checks prevent stale control operations. Time is caller-provided
monotonic ticks and must use the same scale for deadlines and cache expiry.

QVM executes a bounded portable register program; JIT executes validated MIR in
native memory; OIX compiles/runs a small native guest OS; generic KVM delegates
guest register/device setup to explicit callbacks. These adapters consume their
configured programs, not the process `argv` as a host command. Process identity,
environment and working directory remain metadata unless a backend implements
those services. CPU quotas and memory requests are accepted only as documented
by the selected adapter. Native KVM exit budgets cannot interrupt an individual
KVM_RUN that never exits. JIT/host callbacks execute in the embedding process.

Rootfs exports and FUSE dispatch are userspace records. The stack does not
implicitly create host mounts, namespaces, cgroups, veths or privileged processes.
Use the explicit backend hooks for application-specific operating-system integration.

## Verification

`tests/test_container_stack.c` exercises composition and malformed/boundary input.
`tests/test_container_alloc.c` injects allocation failures and checks preserved
ownership across CT, VFS, archive and buffer allocations, including daemon creation.
`tests/test_container_wire.c` applies deterministic truncations and mutations to
image/config/USTAR/ELF/Zstandard/IPC inputs, repairs checksums to reach inner
parsers, and checks streaming and roundtrip invariants. Define
`MULIB_CONTAINER_FUZZ` to use its `LLVMFuzzerTestOneInput` with Clang libFuzzer.
Lifecycle regressions cover paused exits, generation exhaustion, backend failure,
and callback-acquired leases during progressive shutdown.
`tests/test_container_backends.c` runs native MIR, compiles OIX images,
and checks deterministic KVM failure paths. The focused checker verifies standalone
C implementations, C/C++ declarations, include-order composition, examples, and
optional system `zstd`/`readelf` interoperability. KVM guest execution requires an
accessible host device and is not implied by a successful image-compilation test.
