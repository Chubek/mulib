# Mach-O builder

Include the canonical `mulib-machobuilder.h`. Define `MULIB_IMPL_MACHOBUILDER`,
`MULIB_IMPL_OBJFILEBUILDER`, `MULIB_IMPL_OBJFILEWRITER`, `MULIB_IMPL_BINFILEWRITER`,
`MULIB_IMPL_BYCUTILS`, `MULIB_IMPL_BUFFER`. The canonical switch also enables the
historical `mulib_machobuilder_text` convenience function; either entry header's
switch can enable that function without duplicate definitions when both are set.
[Example](../examples/modules/machobuilder.c).

Zero-initialize `mulib_machobuilder_t`. `add` copies unique 1..16-byte names and
arbitrary payloads, accepting NULL only for zero-size payloads. Alignment must
be a power of two at most 32768. Flags are shared WRITE/ALLOC/EXEC bits 1/2/4;
EXEC sections are named in `__TEXT`, other sections in `__DATA`. Mach-O segment
permissions are a permissive object-file profile, not an executable memory policy.

`finish(builder, cpu, out)` atomically appends a deterministic little-endian
Mach-O64 MH_OBJECT. CPU is x86-64 `0x01000007` or arm64 `0x0100000c`.
There is one unnamed LC_SEGMENT_64, at most 255 sections, no symbols, relocations,
entry command, dyld metadata or code signature. File offsets/total size must fit
32 bits. This is not a runnable macOS image.

Functions return 1/0; failures leave output unchanged. `finish` does not consume
the builder; `free` resets it. Output is an ordinary owned `mulib_buffer_t`.
The historical misspelled header/switch remain valid and unchanged for the
single-text-section API. See [shared contracts](binary-formats.md).
