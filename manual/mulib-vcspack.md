# vcspack — Git PACK v2 stored-block profile

[Manuals](README.md) · [Library index](index.md)

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

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-vcspack.h`](../include/mulib-vcspack.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-vcspack.h
 * @brief Git PACK v2 encoding and bounded stored-DEFLATE pack decoding.
 * Define MULIB_IMPL_VCSPACK, GIT, ENDIAN and ADLER32 once. Emits real Git
 * packs; decoder supports non-delta objects with uncompressed zlib blocks.
 */
#ifndef MULIB_VCSPACK_H
#define MULIB_VCSPACK_H
#include "mulib-adler32.h"
#include "mulib-endian.h"
#include "mulib-git.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_vcspack_object
  {
    unsigned type; /* 1 commit, 2 tree, 3 blob, 4 tag */
    const void *data;
    size_t length;
  } mulib_vcspack_object_t;
  /** @brief Encode malloc-owned pack. 1 success/0 invalid, overflow or OOM.
   * Outputs unchanged on error. Inputs borrowed, <=UINT32_MAX objects. */
  int mulib_vcspack_encode (const mulib_vcspack_object_t *, size_t,
                            unsigned char **, size_t *);
  /** @brief Verify pack SHA-1 and zlib checksums then visit each object,
   * borrowing scratch bytes during callback. 1 complete, 0 malformed, -1
   * unsupported compression/delta, -2 object exceeds scratch, -3 callback
   * stopped. Earlier callbacks may have run before a later object fails. */
  int mulib_vcspack_decode (const void *, size_t, void *scratch, size_t,
                            int (*visit) (void *,
                                          const mulib_vcspack_object_t *),
                            void *);
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
| Header | [`include/mulib-vcspack.h`](../include/mulib-vcspack.h) |
| Implementation switch | `MULIB_IMPL_VCSPACK` |
| Repository-local includes | [mulib-adler32](mulib-adler32.md), [mulib-endian](mulib-endian.md), [mulib-git](mulib-git.md) |
| Complete example | [`examples/modules/vcspack.c`](../examples/modules/vcspack.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/vcspack.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
