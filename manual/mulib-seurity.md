# Security byte helpers

[Manuals](README.md) · [Library index](index.md)

Historical filename and `MULIB_IMPL_SEURITY` remain supported; the canonical
switch is `MULIB_IMPL_SECURITY`. No dependencies. Token checks, ASCII equality,
byte comparison without early exit and strict path decoding are provided.
Not cryptography or a compiler-independent constant-time guarantee. Path failure
leaves output unchanged; see [path policy](storage-server.md#http-and-policies)
and [example](../examples/modules/seurity.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-seurity.h`](../include/mulib-seurity.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-seurity.h
 * @brief Security-oriented byte comparison, HTTP tokens and safe origin paths.
 * Historical filename retained. No cryptography or authentication policy.
 * Define MULIB_IMPL_SEURITY (or the historical MULIB_IMPL_SEURITY) once. */
#ifndef MULIB_SEURITY_H
#define MULIB_SEURITY_H
#include <stddef.h>
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_security_equal (const void *a, const void *b, size_t length);
  int mulib_security_token (const char *text);
  /** Decode percent escapes once, reject controls, backslashes, query/fragment,
   * encoded separators, dot segments and repeated slashes. Absolute paths only.
   * Output may alias input, is NUL terminated and unchanged on failure. */
  int mulib_security_path (const char *source, char *out, size_t capacity);
  int mulib_security_ascii_equal (const char *a, const char *b);
#ifdef __cplusplus
}
#endif
#endif
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-seurity.h`](../include/mulib-seurity.h) |
| Implementation switch | `MULIB_IMPL_SEURITY` |
| Repository-local includes | None |
| Complete example | [`examples/modules/seurity.c`](../examples/modules/seurity.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/seurity.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
