# mulib-shoebox-contract

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_SHOEBOX_CONTRACT` once. No dependencies or allocations.
[Example](../examples/modules/shoebox-contract.c).

Shoebox is a planned BusyBox-style multicall utility host. This module defines
**only its contract and host helpers**; it installs no utilities. Version 1 is
a MuLIB interface, not an external/POSIX standard or a BusyBox binary ABI.

## Applet and host obligations

A descriptor contains ABI version, struct_size, canonical name, optional help
metadata and a main callback. Set version to `MULIB_SHOEBOX_ABI_VERSION` and size
to `sizeof(descriptor)`. Version 1 descriptor arrays use exactly that stride;
larger/future descriptors are rejected. Contexts can have a larger compatible
tail, but must have version 1 and at least the known size. This is a same-process,
same-platform C ABI, not a serialized or cross-architecture plugin format.

The context borrows I/O callbacks, an explicit-length environment array and an
opaque host pointer. Applets receive argc plus borrowed immutable argv entries
(no trailing NULL is required by this contract), return 0–255, release all their
own resources and must not call exit or retain invocation pointers. No privilege,
locale, signal, thread-safety or sandbox guarantee is implied. Hosts establish
those policies. Repeated invocation should not depend on mutated global state.

`validate` checks versions, sizes, non-NULL callbacks, names and duplicate names.
Names use ASCII letters/digits, `_-.[]`, cannot begin `-`, and cannot be `.` or
`..`. Empty registries are valid. Duplicate detection is quadratic; `find`
validates then returns an exact-name borrowed descriptor or NULL.

`dispatch` uses basename(argv[0]) for symlink/hardlink invocation. If it matches
the caller's launcher name, argv[1] selects the applet and the callback sees a
shifted argv/argc. No argv storage is mutated. Launcher arguments are not shell
parsed; no automatic help/list/version applet is provided. Use descriptor help
metadata in your eventual host. Unknown names return NOT_FOUND and exit 127;
other contract failures set exit 125. A valid applet return is passed separately
through exit_status, so applet failure and host contract failure are distinct.

## I/O and environment

Logical streams 0/1/2 denote stdin/stdout/stderr; they need not be native file
descriptors. The host owns them throughout. Callbacks report result plus byte
progress, supporting redirection, UEFI adapters, memory streams or POSIX files.
Read success with zero bytes means EOF. `write_all` handles partial writes,
cancellation and zero-progress failure. It reports bytes already written even
on errors and rejects progress greater than requested. Callback-specific errors
map to IO; interrupted/retryable operations are handled by the host callback.

`getenv` searches only the supplied environment, returning the first exact
NAME= value. No global environment is read or changed. Cancellation is checked
before dispatch and before each write. Applets control checks during their own
computation.

Results: OK=0, INVALID=-1, ABI=-2, DUPLICATE=-3, NOT_FOUND=-4, IO=-5,
CANCELLED=-6. Tests use contract probes (not shipped applets) to verify direct
and multicall dispatch, exit codes, duplicate/ABI rejection and partial writes.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-shoebox-contract.h`](../include/mulib-shoebox-contract.h) |
| Implementation switch | `MULIB_IMPL_SHOEBOX_CONTRACT` |
| Repository-local includes | None |
| Complete example | [`examples/modules/shoebox-contract.c`](../examples/modules/shoebox-contract.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/shoebox-contract.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
