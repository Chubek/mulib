# vcdiff — RFC 3284 deltas

Define `MULIB_IMPL_VCDIFF`; no other implementation switches are required.
See [`examples/modules/vcdiff.c`](../examples/modules/vcdiff.c).

`decode` implements the RFC 3284 default 256-entry code table, including paired
instructions, SELF/HERE/NEAR/SAME addressing, four near-cache and 768 same-cache
entries, overlapping target COPY, source windows, previous-target windows, and
multiple windows. Integer parsing is overflow-checked base-128 big-endian
VCDIFF encoding, not LEB128. All section sizes and consumed bytes must agree.
Source COPY cannot cross the source-segment boundary. COPY from target must
start in already produced data. Output capacity bounds decompression.

The decoder returns 1 success, 0 malformed, -1 unsupported feature, or -2
insufficient capacity. `used` is zero on failure, but output may contain a
partial result. Header extensions, custom tables, secondary compression,
nonzero version bytes and nonstandard checksums are explicitly unsupported.
A header-only delta is a valid empty target.

`encode` emits one standards-compatible window: a common-prefix SELF COPY
when at least four bytes match, followed by ADD for the remainder. It is a
simple differencer, not an optimal compressor. It returns 1/0 and preserves
output on insufficient capacity. `target_length + 128` is a sufficient bound
when representable. Both functions borrow all storage, allocate nothing, and
require disjoint output/source/delta or output/source/target buffers.

Reference: [RFC 3284](https://www.rfc-editor.org/rfc/rfc3284).
Tests: `tests/test_vcs_formats.c`, including a hand-encoded RFC example.
