# Cryptographic hashes and MACs

Define `MULIB_IMPL_CRYPTOHASH` and its dependency `MULIB_IMPL_SHA` in exactly
one translation unit. Both headers have C11 and C++17 declarations with C
linkage. These are new native implementations, verified against reference
vectors; they have not received an independent security audit.

BLAKE2s and BLAKE2b implement RFC 7693 sequential hashing and keyed hashing.
Choose a digest length of 1..32 or 1..64 bytes respectively, and a key length
of 0..32 or 0..64. A zero key length selects unkeyed hashing. Output length is
part of initialization, so shorter outputs differ from a truncated full digest.
Call `mulib_blake2s_init` or `mulib_blake2b_init`, `mulib_blake2_update`, then
`mulib_blake2_final`, or use the one-shot functions. Final consumes and wipes
the context and writes exactly the initialized output length. Salt,
personalization, and the BLAKE2 parallel tree parameter block are not exposed.

BLAKE3 implements the complete standard tree construction, keyed mode with a
32-byte key, derive-key mode with a length-delimited application context, and
extendable output. One-shot and streaming APIs are available. BLAKE3 final is
a repeatable snapshot; it permits subsequent input updates. `final_seek`
selects a byte offset in the XOF output. Input length and seek ranges are
limited to `uint64_t` bytes. The context holds the tree stack and allocates no
heap memory. Derive-key contexts are application-specific public strings, and
key material is the message input in that mode.

KMAC128/256 and KMACXOF128/256 follow NIST SP 800-185. Keys, messages, and
customization strings accept arbitrary bytes. Fixed-output KMAC encodes the
requested output length; KMACXOF encodes zero and preserves the output prefix
when more bytes are requested. Key/customization/output lengths must fit in
`UINT64_MAX` bits. The APIs are one-shot and output raw bytes. Empty keys are
supported by the construction; applications must supply appropriate secret
key material for authentication.

SipHash-2-4 and SipHash-1-3 take a 16-byte key and write an 8-byte tag in little
endian. SipHash-1-3 is the reduced-round variant. PMAC implements original PMAC1
with AES-128, AES-192, or AES-256, taking a 16/24/32-byte key and writing a
16-byte tag. Both expose one-shot APIs. PMAC does not use padding conventions
from CMAC or HMAC. Verify MACs with a suitable constant-time comparison.

All status APIs return 1 on success, 0 on invalid arguments or state. NULL
buffers are allowed only for zero-length fields, except fixed-size keys and
tags, which must always be present. Output must not overlap any input or the
context. Failure does not change output. Wipe abandoned or finalized snapshot
contexts with `mulib_cryptohash_wipe`. This is a volatile best-effort wipe and
cannot erase compiler/hardware copies.

The `_intrinsics` APIs use SSE2 for BLAKE2/BLAKE3 and KMAC, or AES-NI for PMAC
when compiled with `-maes`; otherwise they fall back to portable code. Streaming
BLAKE contexts have corresponding initialization alternatives. SSE2 is normally
available on x86-64. There is no runtime feature detection: target flags must
match the executing CPU. `MULIB_HASH_NO_INTRINSICS` forces portable code.
Portable AES computes its S-box algebraically with a fixed operation count and
has no secret-indexed lookup table. Compiler/hardware side-channel behavior
still requires evaluation for the deployment. Intrinsic paths do not imply a
performance improvement on every workload.

`tests/test-hashes.c` includes all official BLAKE3 vectors in all three modes,
all published PMAC AES key-size examples, NIST KMAC and KMACXOF samples, all 64
SipHash-2-4 vectors, variable-length/keyed BLAKE2 results from Python `hashlib`,
streaming boundaries, output sentinels, and intrinsic/portable comparisons.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-cryptohash.h`](../include/mulib-cryptohash.h) |
| Implementation switch | `MULIB_IMPL_CRYPTOHASH` |
| Repository-local includes | [`include/detail/mulib-hash-core.h`](../include/detail/mulib-hash-core.h), [mulib-sha](mulib-sha.md) |
| Complete example | [`examples/modules/cryptohash.c`](../examples/modules/cryptohash.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/cryptohash.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
