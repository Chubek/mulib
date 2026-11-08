# mulib-memtest

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_MEMTEST` once; no dependencies or allocations.
[Example](../examples/modules/memtest.c).

**Destructive:** every word of the caller's aligned, writable, exclusively owned
`uint64_t` region is overwritten. Never pass live code, stack, firmware, MMIO,
page tables, reserved RAM or memory used concurrently by another CPU/device.
Counts are words, not bytes. Empty regions are permitted; passes must be nonzero.
Overflowing byte or counter bounds are rejected before touching memory.

Each pass writes and verifies 67 patterns: zero, all ones, index/pass-derived,
then each of 64 walking ones. Volatile accesses prevent elision but do not bypass
CPU caches. Optional flush callback handles platform cache maintenance before
verification. Optional checkpoint then runs once per pattern and can cancel.
Callbacks borrow the supplied context. They may be used for progress/watchdog
service; they must not accidentally overwrite the tested region.

Status: 0 clean, 1 cancelled, 2 completed with mismatches, -1 invalid input.
The required result records actual read/write counts, total errors, and first
mismatch offset/expected/actual. `first_word == SIZE_MAX` means none.
Cancellation takes precedence over accumulated errors, which remain in result.
Partial writes remain after cancellation; original contents are never restored.
This is a software pattern test, not certification, a Rowhammer test, ECC telemetry
or proof that all physical DRAM was accessed. Test only expendable allocations.

`mulib_memtest_march` adds March C− with ascending and descending reads/writes:
`w0; up(r0,w1); up(r1,w0); down(r0,w1); down(r1,w0); r0`.
Each pass performs five reads and five writes per word. Read/modify operations
are interleaved to exercise coupling and address faults. Hooks run after each of
six sweeps; cancellation leaves partial state, and completed passes leave zeros.
Cache-maintenance and exclusive-memory requirements are identical to `run`.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-memtest.h`](../include/mulib-memtest.h) |
| Implementation switch | `MULIB_IMPL_MEMTEST` |
| Repository-local includes | None |
| Complete example | [`examples/modules/memtest.c`](../examples/modules/memtest.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/memtest.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
