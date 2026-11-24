# Secret audit

Verify HMAC-chained audit records with a dedicated key. The caller must persist and independently anchor the expected head; a chain alone does not detect truncation or rollback. Log pseudonymous metadata, not secret values or protocol payloads. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-secret-audit.h`](../kernels/mulib-secret-audit.h) |
| Implementation switch | `MULIB_IMPL_SECRET_AUDIT` |
| Repository-local includes | [mulib-secret-crypto](mulib-secret-crypto.md) |
| Complete example | [`examples/mulib-secret-kernels.c`](../examples/mulib-secret-kernels.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Ikernels examples/mulib-secret-kernels.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
