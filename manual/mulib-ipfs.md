# ipfs — canonical content identifiers

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_VARINT` and `MULIB_IMPL_IPFS`.
See [`examples/modules/ipfs.c`](../examples/modules/ipfs.c).

The supported CID profile is version 1, raw (0x55) or dag-pb (0x70) codec,
sha2-256 multihash (0x12), 32-byte digest. The public value copies codec and
digest. Binary encoding is exactly 36 bytes with minimal varints; binary decode
requires that exact canonical representation. Text uses lowercase base32
multibase prefix `b`, exactly 59 characters, with unused final bits zero.
Format writes 60 bytes including NUL. CIDv0, base58, other hashes/codecs and
noncanonical encodings are rejected rather than normalized.

All codecs return 1 success or 0 invalid. Caller output remains unchanged on
failure. `verify` delegates actual SHA-256 to a caller callback, then compares
all 32 digest bytes; return 1 only on a matching block. The hash callback must
return 1 and fill the digest, and receives borrowed context/data. Encoding a
CID alone does not establish that any supplied bytes have that content hash.

No allocations, block store, daemon, DHT, Bitswap, DAG traversal or network
fetches are implicit. Compose identifiers with transport/storage APIs as needed.
Tests include the independently known CID of an empty raw block and reject
incorrect trailing base32 bits. See `tests/test_protocol_extensions.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-ipfs.h`](../include/mulib-ipfs.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-ipfs.h
 * @brief IPFS CIDv1 raw/dag-pb SHA-256 identifiers and verified block
 * callbacks. Define MULIB_IMPL_IPFS and MULIB_IMPL_VARINT once. No
 * daemon/DHT/network or SHA-256 implementation is implicit; block verification
 * uses caller hashing.
 */
#ifndef MULIB_IPFS_H
#define MULIB_IPFS_H
#include "mulib-varint.h"
#ifdef __cplusplus
extern "C"
{
#endif
  typedef struct mulib_ipfs_cid
  {
    uint64_t codec;
    unsigned char digest[32];
  } mulib_ipfs_cid_t;
  /** @brief Canonical binary CIDv1 (36 bytes), codecs raw=0x55, dag-pb=0x70,
   * multihash sha2-256=0x12, digest length=32. 1 success/0 malformed. */
  int mulib_ipfs_cid_encode (const mulib_ipfs_cid_t *, unsigned char out[36]);
  int mulib_ipfs_cid_decode (const void *, size_t, mulib_ipfs_cid_t *);
  /** @brief Lowercase base32 multibase text (59 characters plus NUL), exact
   * canonical syntax only. No CIDv0/base58. Outputs unchanged on failure. */
  int mulib_ipfs_cid_format (const mulib_ipfs_cid_t *, char out[60]);
  int mulib_ipfs_cid_parse (const char *, size_t, mulib_ipfs_cid_t *);
  /** @brief Hash a block with caller SHA-256 and compare all 32 bytes.
   * Callback returns 1 on success. 1 match, 0 mismatch/backend/argument
   * failure. */
  int mulib_ipfs_verify (const mulib_ipfs_cid_t *, const void *, size_t,
                         int (*sha256) (void *, const void *, size_t,
                                        unsigned char[32]),
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
| Header | [`include/mulib-ipfs.h`](../include/mulib-ipfs.h) |
| Implementation switch | `MULIB_IMPL_IPFS` |
| Repository-local includes | [mulib-varint](mulib-varint.md) |
| Complete example | [`examples/modules/ipfs.c`](../examples/modules/ipfs.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/ipfs.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
