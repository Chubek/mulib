# Secret selftest

Runs development smoke tests using synthetic data, and returns zero or a numbered failure. Use the [CMake test targets](secret-kernels.md#build-and-verification) to test actual backend linkage and runtime behavior. A passing result is not a security audit.

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-secret-selftest.h`](../include/mulib-secret-selftest.h) |
| Implementation switch | `MULIB_IMPL_SECRET_SELFTEST` |
| Repository-local includes | [mulib-secret-audit](mulib-secret-audit.md), [mulib-secret-aws](mulib-secret-aws.md), [mulib-secret-catalog](mulib-secret-catalog.md), [mulib-secret-codec](mulib-secret-codec.md), [mulib-secret-jose](mulib-secret-jose.md), [mulib-secret-lease](mulib-secret-lease.md), [mulib-secret-oauth](mulib-secret-oauth.md), [mulib-secret-otp-uri](mulib-secret-otp-uri.md), [mulib-secret-otp](mulib-secret-otp.md), [mulib-secret-passphrase](mulib-secret-passphrase.md), [mulib-secret-provider](mulib-secret-provider.md), [mulib-secret-random](mulib-secret-random.md), [mulib-secret-rate](mulib-secret-rate.md), [mulib-secret-recovery](mulib-secret-recovery.md), [mulib-secret-spec](mulib-secret-spec.md), [mulib-secret-ssh](mulib-secret-ssh.md), [mulib-secret-vault](mulib-secret-vault.md), [mulib-secret-webauthn](mulib-secret-webauthn.md) |
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
