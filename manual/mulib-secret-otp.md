# Secret OTP

HOTP, TOTP and Steam generation are local primitives. Durable HOTP issuance requires the host to commit the reservation before releasing a code. The bounded TOTP verifier rejects previously accepted or ambiguous time steps using caller-supplied CAS; the host owns authentication, rate limiting, trusted time and durable replay state. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`kernels/mulib-secret-otp.h`](../kernels/mulib-secret-otp.h) |
| Implementation switch | `MULIB_IMPL_SECRET_OTP` |
| Repository-local includes | [mulib-secret-crypto](mulib-secret-crypto.md), [mulib-secret-state](mulib-secret-state.md) |
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
