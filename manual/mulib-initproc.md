# initproc

[Manuals](README.md) · [Library index](index.md)

POSIX child-reaping primitive. `reap` waits for one positive child PID or any
child (-1), optionally nonblocking. EINTR retried. Returns 1 reaped (PID and
shell-style exit status outputs), 0 not ready, 2 no matching child, -1 invalid,
-2 OS error, -3 unsupported. Outputs unchanged unless reaped. Signal death maps
to 128+signal; normal termination uses the exit code.

No fork/exec, PID-1 setup, subreaper registration, signal forwarding or handlers.
Caller owns the supervisor loop and its synchronization/signal policy.
Switch: `MULIB_IMPL_INITPROC`. Example: `examples/modules/initproc.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-initproc.h`](../include/mulib-initproc.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-initproc.h
 * @brief POSIX child-reaping primitive for caller-owned init/supervisor loops.
 * Define MULIB_IMPL_INITPROC once. Does not become PID 1, fork, or install
 * handlers. */
#ifndef MULIB_INITPROC_H
#define MULIB_INITPROC_H
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Wait for child pid>0 or any child (-1). nonblocking is 0/1.
   * 1 reaped (outputs pid and shell-style exit status), 0 not ready,
   * 2 no matching children, -1 invalid, -2 OS error, -3 unsupported.
   * EINTR retried. Signal death maps to 128+signal. Outputs unchanged unless
   * reaped. */
  int mulib_initproc_reap (int child, int nonblocking, int *pid,
                           int *exit_status);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-initproc.h`](../include/mulib-initproc.h) |
| Implementation switch | `MULIB_IMPL_INITPROC` |
| Repository-local includes | None |
| Complete example | [`examples/modules/initproc.c`](../examples/modules/initproc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/initproc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
