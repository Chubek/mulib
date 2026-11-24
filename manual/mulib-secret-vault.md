# Secret vault

MSV1 wraps a random development vault root under a password-derived key. Use `mulib_sv_unlock_pinned` with an independently configured profile, never with values taken from the untrusted header. Rewrapping preserves the root and cannot revoke old envelopes. Secure memory, persistence and lifecycle still need host integration and review. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-secret-vault.h`](../kernels/mulib-secret-vault.h) |
| Implementation switch | `MULIB_IMPL_SECRET_VAULT` |
| Repository-local includes | [mulib-secret-memory](mulib-secret-memory.md), [mulib-secret-record](mulib-secret-record.md) |
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
