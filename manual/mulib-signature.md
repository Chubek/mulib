# signature — RSA and ECDSA verification

**Header:** `include/mulib-signature.h` · **Switch:** `MULIB_IMPL_SIGNATURE`

Also define `DER`, `SHA` and `ENTROPY` implementation switches. This module
verifies signatures using public data. It has no signing, private RSA operation
or private elliptic-curve scalar API. Its bounded Montgomery/public-point
arithmetic is variable-time and must not be reused for private-key operations.

`mulib_public_key_t` borrows its key buffers:

| kind | Key encoding |
| --- | --- |
| 1 | RSA unsigned big-endian modulus (2048–4096 bits), plus odd exponent 3..2^32-1 |
| 23 | P-256 uncompressed SEC1 point, 65 bytes |
| 24 | P-384 uncompressed SEC1 point, 97 bytes |

`mulib_signature_verify(key,scheme,message,length,signature,signature_length)`
returns 1 only for a valid signature, otherwise 0. NULL message is accepted at
length zero. Signature schemes use TLS numeric identifiers:

| Schemes | Verification |
| --- | --- |
| 0x0401 / 0x0501 / 0x0601 | RSA PKCS#1 v1.5 with SHA-256 / 384 / 512 |
| 0x0804 / 0x0805 / 0x0806 | RSA-PSS, matching MGF1 hash and digest-sized salt |
| 0x0403 / 0x0503 | ECDSA with SHA-256 / SHA-384 |

RSA signatures must exactly match modulus byte length and encode an integer
strictly below the modulus. PKCS#1 padding/DigestInfo is checked exactly. PSS
checks the encoded bit length, mask, zero padding, delimiter, salt and trailer.
RSA-PSS-constrained SPKI keys are not supported by the X.509 parser.

ECDSA accepts a strictly encoded DER SEQUENCE of two positive INTEGERs, each
strictly within the curve order. Public coordinates must be within the field
and lie on the curve. Both low-S and high-S signatures are accepted. The generic
verifier allows either supported hash with either curve for certificate
signatures; TLS CertificateVerify additionally enforces each TLS scheme's curve.

[`examples/modules/signature.c`](../examples/modules/signature.c) uses the
RFC 6979 P-256/SHA-256 vector. `tests/test-signature.c` and
`tests/test_tls_integration.py` compare all RSA hash/padding combinations and both
curves with independently generated OpenSSL signatures, reject changed messages
and signatures, and reject incorrect PSS salt lengths. No OpenSSL code is linked
into MuLIB.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-signature.h`](../include/mulib-signature.h) |
| Implementation switch | `MULIB_IMPL_SIGNATURE` |
| Repository-local includes | [mulib-der](mulib-der.md), [mulib-entropy](mulib-entropy.md), [mulib-sha](mulib-sha.md) |
| Complete example | [`examples/modules/signature.c`](../examples/modules/signature.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/signature.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
