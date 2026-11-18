# mulib-landlock

## Build
`MULIB_IMPL_LANDLOCK`, `MULIB_IMPL_SANDBOX_PROFILE`; no liblandlock dependency.
Linux UAPI syscalls, `-D_GNU_SOURCE`. Unsupported targets report unsupported.

## API
`mulib_landlock_abi()` returns a positive ABI or negative error.
`mulib_landlock_supported_access(abi)` returns the filesystem rights known
through ABI 5 (ABI 1 base, ABI 2 REFER, ABI 3 TRUNCATE, ABI 5 IOCTL_DEV).
`mulib_landlock_restrict(rules,count,min_abi)` checks kernel support and each
rule's actual fd/type/access; creates a ruleset handling all known supported
filesystem rights and adds each path-beneath rule. Applies no_new_privs and
restrict_self, closes its own ruleset fd, borrows every caller rule fd. Passing
zero rules denies handled access, not all system calls. An unsupported access
mask or requested ABI is rejected; no silent truncation. Max rules=4096.

Open paths using `O_PATH|O_CLOEXEC` under trusted installation roots. An fd
rather than a string pins the intended filesystem object; the caller must
validate its identity and any complex hierarchy. Rules are additive and
inherited. Do not add a broad root rule if you intend to restrict reads.
The pre-exec launcher requires rules allowing its trusted executable and loader
access. Runtime strict seccomp handles remaining path operations. Direct tests
use an allowed private directory, then verify reads elsewhere and writable
opens are denied on an enforcing host.

Landlock is irreversible and operates only on handled rights. It cannot revoke
already-open descriptors, and this module does not cover all metadata,
network/signal scopes, or future UAPI rights. Keep the caller in a dedicated
child. Failure after no_new_privs is not a rollback. Abort rather than proceed.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-landlock.h`](../include/mulib-landlock.h) |
| Implementation switch | `MULIB_IMPL_LANDLOCK` |
| Repository-local includes | [mulib-sandbox-profile](mulib-sandbox-profile.md) |
| Complete example | [`examples/modules/sandbox-process.c`](../examples/modules/sandbox-process.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sandbox-process.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
