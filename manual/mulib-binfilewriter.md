# binfilewriter

Writes to an initialized shared byte buffer. `uint` appends unsigned LE integers
of width 1..8; `align` appends zero bytes to a power-of-two alignment; `patch`
changes an existing bounded field. All return 1 success, 0 invalid/overflow/OOM
and preserve logical output on failure. Values must fit requested width.
Alignment uses existing capacity directly and performs at most one realloc when
growth is needed; it does not allocate a separate padding buffer. Zero-fill and
length changes occur only after growth succeeds. Align/patch reject inconsistent
buffer length/capacity metadata.

No native format structs, external files or automatic finalization. Caller owns
the buffer and frees it with `mulib_buffer_free`.
Switches: `MULIB_IMPL_BINFILEWRITER`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`.
Example: `examples/modules/binfilewriter.c`.
