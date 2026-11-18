# mulib-sandbox-profile

## Build
`MULIB_IMPL_SANDBOX_PROFILE`; no dependency. Portable C11.

## API
`mulib_sandbox_profile_default()` returns version 1 with required seccomp,
rlimits and credential restriction. Defaults: 256 MiB address space, 8 MiB
stack, 30 CPU seconds, 60 s execution, 3 s startup, 2 MiB frame, 256 broker
requests and 16 MiB cumulative broker payload. Cgroup defaults are 256 MiB
memory, one process, 100 ms quota/100 ms period. Optional enforcement is off
unless selected, not claimed active. Landlock default minimum ABI is 3.

`mulib_sandbox_profile_validate` checks version, required masks, namespace
consistency and nonzero/bounded values. A strict worker must retain exactly
FD 3 and three standard descriptors and cannot create other processes, so
version 1 requires max processes=1 and max open files=4. Setup FD 4 exists only
before enforcement. Memory and CPU numbers are architecture/OS constraints,
not a replacement for cgroup enforcement. `mulib_sandbox_strerror` returns
static text. These functions do not enforce the policy themselves.

Status codes (shared): 0 success, 1 again, 2 commit uncertain, negative errors
for invalid, system, unsupported, denied, limit, deadline, protocol, allocation,
state, EOF. Treat nonzero as non-success unless an API specifies otherwise.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sandbox-profile.h`](../include/mulib-sandbox-profile.h) |
| Implementation switch | `MULIB_IMPL_SANDBOX_PROFILE` |
| Repository-local includes | None |
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
