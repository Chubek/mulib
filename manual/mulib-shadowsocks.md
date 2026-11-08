# shadowsocks — SIP004 AEAD TCP chunks

[Manuals](README.md) · [Library index](index.md)

Define `MULIB_IMPL_SHADOWSOCKS`; no MuLIB dependencies.
See [`examples/modules/shadowsocks.c`](../examples/modules/shadowsocks.c).

The module frames SIP004 chunks: encrypted two-byte payload length plus 16-byte
tag, followed by encrypted payload plus 16-byte tag. Payload length is at most
0x3fff; empty chunks are accepted. An explicit backend implements AES-GCM or
ChaCha20-Poly1305 with 12-byte nonce and no AAD. Caller code obtains/transmits
fresh salts, derives direction-specific subkeys with HKDF-SHA1, and initializes
one separate state per direction. Salt reuse and subkey reuse are prohibited
by the protocol; this framing layer does not generate or track salts.

`init` borrows the cipher/context and selects seal/open mode. Callbacks return
1 on success, 0 on failure, and must obey fixed output sizes. Seal input length
n produces n+16 bytes; open input includes the tag and produces n-16 bytes.
Callbacks are stateless with respect to the explicit nonce and may be called
again when an incomplete frame is retried. Nonces increment little-endian,
two per chunk, using a deliberately bounded uint64 counter with the high four
bytes zero; exhaustion fails before reuse.

Seal returns 1 success, -1 invalid/capacity, -2 sticky backend/exhaustion error.
Open adds 0 incomplete, authenticates both records into temporary storage before
publishing plaintext, and advances counters only after full success. Invalid
authenticated length or tag failure is sticky. `used`/`count` reset on errors;
seal output may be partially written on backend failure. Buffers must not
overlap. UDP, legacy stream ciphers and Shadowsocks 2022 are outside this API.
Tests use explicitly noncryptographic fixtures to verify framing and nonce
policy; real cryptography must come from the backend.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-shadowsocks.h`](../include/mulib-shadowsocks.h) |
| Implementation switch | `MULIB_IMPL_SHADOWSOCKS` |
| Repository-local includes | None |
| Complete example | [`examples/modules/shadowsocks.c`](../examples/modules/shadowsocks.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/shadowsocks.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
