# entropy — OS cryptographic random bytes and secret buffers

**Header:** `include/mulib-entropy.h` · **Switch:** `MULIB_IMPL_ENTROPY`

`mulib_entropy(output,length)` fills the complete buffer and returns 1. Linux
uses blocking `getrandom`, retrying interrupted calls; other POSIX platforms read
`/dev/urandom` through a close-on-exec descriptor. There is no userspace PRNG or
time-based fallback. Unsupported platforms return 0. Any partial failure wipes
the entire destination. NULL is valid only at zero length; zero length succeeds.
No allocation, global state or MuLIB dependencies are needed.

`mulib_secret_wipe(pointer,length)` uses volatile byte writes. NULL is accepted
and ignored. `mulib_secret_equal(a,b,length)` compares all bytes with a fixed
operation count for a public length, returning 1 for equality and 0 otherwise.
NULL inputs are valid at zero length. These utilities do not lock pages, prevent
core dumps or erase copies made elsewhere by the application.

Use entropy to generate ephemeral X25519 scalars and fresh symmetric keys. Nonce
uniqueness for a long-lived AES-GCM key remains a caller-managed requirement.
See [`examples/modules/entropy.c`](../examples/modules/entropy.c) and the
[TLS client](mulib-tls.md), which gets new entropy for every handshake.

```sh
cc -std=c11 -Wall -Wextra -Werror -Iinclude examples/modules/entropy.c -o entropy
./entropy
```

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-entropy.h`](../include/mulib-entropy.h) |
| Implementation switch | `MULIB_IMPL_ENTROPY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/entropy.c`](../examples/modules/entropy.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/entropy.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
