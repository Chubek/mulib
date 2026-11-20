# Secret OTP URI

Import through the deliberately restricted `otpauth://` profile. The URI and parsed key are secrets; never log them. This is not QR transport or vendor migration support. See [secret kernels](secret-kernels.md).

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-secret-otp-uri.h`](../include/mulib-secret-otp-uri.h) |
| Implementation switch | `MULIB_IMPL_SECRET_OTP_URI` |
| Repository-local includes | [mulib-secret-codec](mulib-secret-codec.md), [mulib-secret-otp](mulib-secret-otp.md) |
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
