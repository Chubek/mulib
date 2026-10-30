# vcspack — Git PACK v2 stored-block profile

Implementation switches: `MULIB_IMPL_GIT`, `MULIB_IMPL_ENDIAN`,
`MULIB_IMPL_ADLER32`, `MULIB_IMPL_VCSPACK`.
See [`examples/modules/vcspack.c`](../examples/modules/vcspack.c).

`encode` writes a real Git PACK version 2: object count, variable-length
type/size headers, zlib streams containing stored DEFLATE blocks, and the
SHA-1 pack trailer. Types 1/2/3/4 mean commit/tree/blob/tag. Empty objects are
supported. Input objects are borrowed; output is malloc-owned and must be
freed by the caller. Return 1 success or 0 invalid/overflow/allocation failure;
output arguments are unchanged on failure. Encoding deliberately favors a
dependency-free representation over compression ratio.

`decode` verifies the entire pack SHA-1 before callbacks, then each zlib header,
stored-block length/complement, object size, and Adler-32. The caller supplies
scratch capacity for the largest object. Callback objects borrow scratch and
expire on return. Results: 1 complete, 0 malformed, -1 unsupported compression
or delta type, -2 scratch too small, -3 callback stopped. Earlier objects may
have been delivered before a later object's validation fails; callers wanting
atomic import should stage them separately.

Huffman-compressed streams, REF/OFS deltas, thin packs, indexes and SHA-256
repositories are outside the decoder profile. Unsupported inputs are rejected.
Tests: `tests/test_vcs_formats.c`; its `vcs-test.pack` fixture is also accepted
by `git index-pack --strict`.
