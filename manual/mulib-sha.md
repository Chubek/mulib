# SHA family

Define `MULIB_IMPL_SHA` in one translation unit. The header implements historical
SHA-0 and SHA-1, SHA-224/256/384/512, SHA-512/224 and SHA-512/256, SHA3-224/256/384/512,
and SHAKE128/256. These are byte-oriented implementations of FIPS 180-4 and
FIPS 202; SHA-0 is the historical predecessor. SHA-0 and SHA-1 are for legacy
interoperability and should not be used when collision resistance is required.

`mulib_sha(algorithm, data, length, out, capacity)` writes the fixed digest;
for SHAKE, capacity specifies the exact requested output length. Convenience
functions such as `mulib_sha256` require the indicated fixed output size.
`mulib_sha_digest_size` returns zero for SHAKE and unsupported algorithms.
The digest byte order follows each standard.

For streaming, call `mulib_sha_init`, `mulib_sha_update` any number of times,
then `mulib_sha_final`. Fixed digest finalization requires sufficient capacity
and consumes and wipes the context. SHAKE uses `mulib_sha_squeeze`; successive
calls continue the output stream, including across permutation boundaries.
Updates after squeezing fail. Copy an active context to take a snapshot.

All status APIs return 1 for success, 0 for invalid input or state. NULL input
is accepted at zero length. A fixed digest requires non-NULL output; zero-length
SHAKE output may be NULL. Invalid buffers/capacities do not change output.
Lengths and capacities are in bytes. Input and output must not overlap each
other or the context. No allocation occurs. SHA-1/SHA-256 lengths are limited
to fewer than 2^64 bits and SHA-512 lengths to fewer than 2^128 bits.

`mulib_sha_init_intrinsics` and `mulib_sha_intrinsics` select SSE2 implementations
of SHA-256/SHA-512 message scheduling and SHA-3/Keccak transformations when
compiled with SSE2 support. SHA-0/1 use the portable implementation. There is
no runtime CPU dispatch: compiler target features must match the executing CPU.
`MULIB_HASH_NO_INTRINSICS` disables intrinsic code throughout the hash headers.
No performance improvement is promised on a particular CPU or workload.

The focused regression suite checks Python `hashlib` outputs at padding and
block boundaries, SHA-0 examples, the million-byte SHA-256 vector, streaming,
output bounds, and portable/intrinsic equivalence.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-sha.h`](../include/mulib-sha.h) |
| Implementation switch | `MULIB_IMPL_SHA` |
| Repository-local includes | [`include/detail/mulib-hash-core.h`](../include/detail/mulib-hash-core.h) |
| Complete example | [`examples/modules/sha.c`](../examples/modules/sha.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/sha.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
