# hkdf — HMAC, HKDF and TLS 1.3 labels

**Header:** `include/mulib-hkdf.h` · **Switch:** `MULIB_IMPL_HKDF`

Define the `SHA` and `ENTROPY` implementation switches as well. Supported hash
algorithms are `MULIB_SHA256`, `MULIB_SHA384`, and `MULIB_SHA512`.
Functions return 1 on success and 0 on invalid arguments. There is no allocation.
NULL byte inputs are accepted only for zero-length inputs.

* `mulib_hmac(algorithm,key,key_length,data,length,out,capacity)` implements
  RFC 2104. Capacity must fit the digest (32, 48 or 64 bytes). Output may overlap
  key/input because the result is written after consuming them.
* `mulib_hkdf_extract` implements RFC 5869 Extract. Empty salt is equivalent to
  the specified zero salt. Output capacity must fit the digest.
* `mulib_hkdf_expand` accepts a PRK at least digest-sized and arbitrary binary
  context. Output length is at most `255 * digest_size`; zero length is allowed.
  Output must not overlap PRK or context. Invalid arguments preserve output.
* `mulib_hkdf_tls13(secret,label,context,context_length,out,length)` encodes the
  RFC 8446 HkdfLabel with the `tls13 ` prefix, SHA-256 and a 32-byte secret.
  The NUL-terminated label is at most 249 bytes, context at most 255 bytes, and
  output at most 8160 bytes. Lengths in the encoded label are big-endian.

Intermediate HMAC contexts and digest buffers are wiped. Caller-owned keys,
PRKs and output secrets remain caller-owned and should be wiped when retired.
The module is key derivation, not a password-hardening function.

[`examples/modules/hkdf.c`](../examples/modules/hkdf.c) derives a labelled
application key. `tests/test-tls-crypto.c` covers RFC 5869 and RFC 4231 vectors,
unsupported algorithms and the Expand length limit. TLS interoperability tests
exercise the complete TLS 1.3 key schedule.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-hkdf.h`](../include/mulib-hkdf.h) |
| Implementation switch | `MULIB_IMPL_HKDF` |
| Repository-local includes | [mulib-entropy](mulib-entropy.md), [mulib-sha](mulib-sha.md) |
| Complete example | [`examples/modules/hkdf.c`](../examples/modules/hkdf.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/hkdf.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
