# mulib-typecore

Public utilities used by the publishing additions. Buffers are dynamically owned and caller-freed using `mulib_typecore_free`. `reserve`/`append` are size-checked, `append` supports self-aliasing, and scalar codecs are bounds-checked. The helper includes UTF-8 decoding, ASCII/markup escaping, big-/little-endian integers, CRC32 and Adler32. The UTF-8 scanner rejects overlong encodings, surrogates and out-of-range scalars. The encoder does not normalize Unicode. 

Define `MULIB_IMPL_TYPECORE` in one translation unit. Requires only C11 and the C standard library. See `tests/test-typecore.c`.

## Allocation and failure handling

Define compatible `MULIB_TYPECORE_REALLOC` and `MULIB_TYPECORE_FREE` hooks before
the implementation include. Defaults are libc `realloc` and `free`; custom
reallocation must preserve the old allocation on failure and provide normal C
allocation alignment. Buffers must be released with `mulib_typecore_free`, not
with an unrelated allocator. The public buffer layout and function ABI are unchanged.

Failed reserve/append operations leave the buffer unchanged. HTML, LaTeX and JSON
escaping now stage their results before appending: failure preserves the caller's
existing bytes and size, including for self-aliased input. This uses temporary
memory proportional to the encoded result. `append` also rebases aliases into the
existing allocation if growth moves it; source bytes must be initialized and
within that allocation. Invalid buffer size/capacity combinations are rejected.
`byte` rejects values above 255 rather than silently truncating them.

CRC32/Adler32 return zero for NULL with nonzero length (not a distinct checksum
error code); NULL with zero length has the normal empty-input checksum.
`tests/test-font-allocations.c` exercises fault injection, ownership and escaping
atomicity. These helpers do not implement Unicode normalization or validate that
arbitrary input bytes form legal XML/HTML text.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-typecore.h`](../include/mulib-typecore.h) |
| Implementation switch | `MULIB_IMPL_TYPECORE` |
| Repository-local includes | None |
| Complete example | [`examples/mulib-typecore.c`](../examples/mulib-typecore.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-typecore.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
