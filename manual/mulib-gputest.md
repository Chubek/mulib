# mulib-gputest

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_GPUTEST` once. No dependencies or allocations.
[Example](../examples/modules/gputest.c) uses an explicitly labeled mock driver.

There is no universal bare-metal GPU command set. This module implements finite
stress-test orchestration; **real GPU execution requires a device-specific
driver** supplied by the application. It does not claim a CPU pixel loop stresses
shader cores. Use `mulib-framebuffer.h` separately for CPU-mapped VRAM patterns.

The backend borrows a context and provides submit, wait, verify and cleanup.
Each iteration uses a deterministic seed and configured work-item count; submit
must launch the actual GPU workload, and verify must compare against an independent
reference. Wait receives a mandatory nonzero timeout in milliseconds and must
enforce it and quiesce work even on failure. Failed submit must leave no live work.
Cleanup runs exactly once after any valid run, including thermal/backend failure.
The module has no clock and cannot police a backend that hangs.

Iterations and work count must be nonzero. An optional sensor reports millidegrees
Celsius before each submission. A nonzero thermal threshold requires the sensor;
reaching the threshold stops before submission. This is not continuous thermal
monitoring: the driver must enforce safety limits during an individual workload.
The sensor is also checked after each completed verification, including the last.
The caller must implement power limits, watchdog and device reset policy.

Return 0 on success, -1 on invalid arguments, -2 on backend failure, -3 on thermal
limit. Required result reports completed verifications, peak observed temperature
and last backend status. Invalid runs do not call cleanup. No real GPU hardware
backend is bundled; hosted tests validate sequencing and failure handling only.

## Executable compute workload

`mulib_gputest_shader()` supplies GLSL 4.30 source for an integer-mixing stress
kernel. Bind a writable std430 uint SSBO at binding zero, set `seed`, `count` and
`rounds` uint uniforms, and dispatch `ceil(count/64)` groups (calculate in 64-bit
arithmetic). Allocate at least count*4 bytes and check device buffer/dispatch
limits. Each invocation writes one result; rounds controls compute intensity.
Zero rounds tests indexing/transfers.

`mulib_gputest_reference` implements the same modulo-2^32 arithmetic on the CPU;
`mulib_gputest_verify` checks synchronized readback and returns mismatch count or
SIZE_MAX for invalid bounds. Synchronize GPU writes/invalidate CPU caches as
required by the driver. The source is suitable for an OpenGL compute backend;
bare-metal drivers must compile/translate it for their hardware. A GPU driver
and shader compiler are still required for real device execution.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-gputest.h`](../include/mulib-gputest.h) |
| Implementation switch | `MULIB_IMPL_GPUTEST` |
| Repository-local includes | None |
| Complete example | [`examples/modules/gputest.c`](../examples/modules/gputest.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/gputest.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
