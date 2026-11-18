# mulib-seccomp

## Build
`MULIB_IMPL_SECCOMP`, `MULIB_IMPL_SANDBOX_PROFILE`; Linux LP64 little-endian
x86-64/AArch64 with seccomp syscall, default-deny kill action, and TSYNC.
Compile with `-D_GNU_SOURCE`. `mulib_seccomp_supported()` probes kill-action
availability; installation itself is the authoritative enforcement check.

## API
Initialize a policy with `mulib_seccomp_init(&policy, errno_value)`; 0 kills
unmatched calls, 1..4095 returns that errno (for diagnostics; not the production
worker default). `mulib_seccomp_allow` appends any correctly identified native
syscall with up to four comparisons; matching rule semantics are OR across
rules, AND across checks. Each comparison is `(args[argument] & mask) == value`
using full 64-bit arguments. No wildcard architecture; x32 syscall numbers
are rejected. `mulib_seccomp_validate` rejects corrupt or unsupported rule
shapes. The bounded compiler emits classic BPF with checked jumps and an
architecture guard, then uses `no_new_privs` and `SECCOMP_FILTER_FLAG_TSYNC`.
Never attempt to install rules in the controlling supervisor. Installation is
irreversible, and can leave no_new_privs set after failure. Terminate on error.

`mulib_seccomp_worker(&policy,3,0)` builds the production broker-only profile.
It permits basic private non-executable memory, clocks, random bytes, signals
within the process, exits and limited allocator helpers. Read/write, sendto
and recvfrom are allowed only on the exact broker descriptor, using the
framing transport's flags and no address arguments; poll/ppoll may wait on
the remaining descriptors. Filesystem acquisition, socket creation,
exec/fork/clone, ptrace, executable/shared/fixed mmap, executable mprotect,
fd passing/duplication, and other syscalls are denied. `mremap` is not admitted;
worker runtimes needing it or additional syscalls require reviewed changes.
This assumes a closed inherited-FD set, no `READ_IMPLIES_EXEC` process
personality, and a trusted bootstrap. Custom rules
can weaken isolation; only trusted policy code builds them.

Seccomp cannot filter arbitrary pointed-to path strings, and is not a path or
network authorization system. Combine with pre-exec restrictions and capability
broker as appropriate. See the worker example and OS denial tests.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-seccomp.h`](../include/mulib-seccomp.h) |
| Implementation switch | `MULIB_IMPL_SECCOMP` |
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
