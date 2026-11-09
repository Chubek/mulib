# mulib-bcj

[Manuals](README.md) · [XZ](mulib-xz.md)

Reversible XZ executable branch/call filters. Define `MULIB_IMPL_BCJ` before
including `mulib-bcj.h` in one translation unit. No dependencies or allocation.
[Complete example](../examples/modules/bcj.c).

`mulib_bcj_transform(id, data, size, position, encode)` transforms a whole block
in place; encode=1 is the forward filter and encode=0 the inverse. Input is owned
by the caller. NULL is valid only for size zero. IDs are the XZ filter IDs:
4=x86, 5=PowerPC, 6=IA64, 7=ARM, 8=ARM Thumb, 9=SPARC. Initial position must be
aligned to 1/4/16/4/2/4 bytes respectively. Position arithmetic wraps at 32 bits
as specified by the filters. Incomplete trailing instructions are untouched.

Return zero on success, -1 for invalid arguments/alignment, or -5 for an
unsupported ID. Validation occurs before mutation. One call processes one complete
block, not an arbitrary streaming fragment: x86's recent-instruction state and
partial instruction boundaries are local to the call. This is a byte transform,
not executable disassembly. ARM64 and RISC-V filters are not currently implemented.
Work is linear in size with constant memory; independent blocks are concurrent.
`tools/check_compression.py` checks XZ streams containing all six filters against
liblzma, including random bytes and overlapping x86 branch-like patterns.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-bcj.h`](../include/mulib-bcj.h) |
| Implementation switch | `MULIB_IMPL_BCJ` |
| Repository-local includes | None |
| Complete example | [`examples/modules/bcj.c`](../examples/modules/bcj.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/bcj.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
