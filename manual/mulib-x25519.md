# x25519 — Curve25519 key agreement

**Header:** `include/mulib-x25519.h` · **Switch:** `MULIB_IMPL_X25519`

Also define `MULIB_IMPL_ENTROPY` for secret-buffer utilities. The implementation
uses 17 radix-2^15 field limbs and a fixed-schedule Montgomery ladder. It needs
only standard 32/64-bit unsigned arithmetic; no compiler-specific wide integer
or external cryptographic dependency is required.

`mulib_x25519_public(out,scalar)` computes a public key using the RFC 7748 base
point. `mulib_x25519(out,scalar,peer)` computes a shared secret. All buffers are
exactly 32 bytes; public keys and shared secrets use little-endian encoding.
Generate each private scalar with `mulib_entropy`. Both entry points clamp the
scalar internally, and peer decoding masks the high bit and reduces noncanonical
field values as required by RFC 7748.

Return values are 1 for success and 0 for invalid pointers or an all-zero shared
secret. Failure preserves output. Exact input/output aliasing is supported.
Internal scalar/field buffers are wiped; caller-owned secrets must be retired
explicitly. Apply a protocol KDF to shared secrets, and authenticate the peer;
X25519 alone provides no identity verification.

[`examples/modules/x25519.c`](../examples/modules/x25519.c) demonstrates two
ephemeral peers. `tests/test-tls-crypto.c` checks RFC 7748 Alice/Bob keys and shared
secret, high-bit masking, zero-secret rejection, aliasing and 1000 iterations.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-x25519.h`](../include/mulib-x25519.h) |
| Implementation switch | `MULIB_IMPL_X25519` |
| Repository-local includes | [mulib-entropy](mulib-entropy.md) |
| Complete example | [`examples/modules/x25519.c`](../examples/modules/x25519.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/x25519.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
