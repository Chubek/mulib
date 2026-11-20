# Secret record

MSR1 authenticates public identity and revision with the ciphertext and uses a per-write derived key. Use `mulib_sr_open_pinned` with the trusted vault algorithm, authenticate the index, and use unique vault roots; MSR1 has no embedded vault UUID. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-secret-record.h`](../include/mulib-secret-record.h) |
| Implementation switch | `MULIB_IMPL_SECRET_RECORD` |
| Repository-local includes | [mulib-secret-crypto](mulib-secret-crypto.md) |
| Complete example | [`examples/mulib-secret-kernels.c`](../examples/mulib-secret-kernels.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/mulib-secret-kernels.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
