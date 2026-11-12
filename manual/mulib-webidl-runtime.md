# webidl-runtime

[Manuals](README.md) · [Web/pricing integration](web-primitives.md)

Strict primitive WebIDL conversion utilities with range validation.

## Contract and supported profile

Integer conversions reject nonfinite values. Strict mode requires an exact
in-range integer; clamp mode saturates and rounds ties to even independently of
the host rounding mode. Boolean conversion is false only for zero/NaN (infinity
is true). domstring_utf8 validates UTF-8 and appends copied bytes, including
embedded NUL; it does not produce UTF-16 or preserve unpaired surrogates. Integer
and buffer outputs remain unchanged on failure. These conservative primitives
are not the full WebIDL/ECMAScript ToNumber or modulo conversion algorithms.

## Integration and ownership

Include `include/mulib-webidl-runtime.h`. Define the following switches in exactly one C11
translation unit, before including the module; do not enable implementations in
every consumer:

`MULIB_IMPL_SHYLOCK_CORE`, `MULIB_IMPL_UTF8`, `MULIB_IMPL_WEBIDL_RUNTIME`.

Only MuLIB and the C standard library are used (link libm where required).
Lengths/offsets are bytes unless explicitly stated above. Public structs must
remain initialized and internally consistent; do not shallow-copy owning objects.
Operations are not thread-safe on shared mutable instances. See the
[common lifecycle and callback rules](web-primitives.md) for fresh storage,
borrowed views, allocation failure and reentrancy requirements.

## Example and verification

The complete [offline example](../examples/modules/webidl-runtime.c) checks results and
releases owned state. Behavioral coverage is in
[`tests/test_web_browser.c`](../tests/test_web_browser.c), with moving-allocation
failure coverage in [test_web_alloc.c](../tests/test_web_alloc.c) and bounded
malformed-input coverage in [test_web_hostile.c](../tests/test_web_hostile.c).

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-webidl-runtime.h`](../include/mulib-webidl-runtime.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-webidl-runtime.h
 * @brief Strict primitive WebIDL conversion utilities with range validation.
 * C11 single-header module. Define MULIB_IMPL_WEBIDL_RUNTIME in exactly one C
 * translation unit. Offers a conservative primitive subset, not complete
 * WebIDL conversion.
 */
#ifndef MULIB_WEBIDL_RUNTIME_H
#define MULIB_WEBIDL_RUNTIME_H
#include "mulib-shylock-core.h"
#include "mulib-utf8.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** @brief See manual/mulib-webidl-runtime.md for this operation's contract.
   */
  int mulib_webidl_to_int32 (double value, int clamp, int32_t *out);
  /** @brief See manual/mulib-webidl-runtime.md for this operation's contract.
   */
  int mulib_webidl_to_uint32 (double value, int clamp, uint32_t *out);
  /** @brief See manual/mulib-webidl-runtime.md for this operation's contract.
   */
  int mulib_webidl_to_boolean (double number);
  /** @brief See manual/mulib-webidl-runtime.md for this operation's contract.
   */
  int mulib_webidl_domstring_utf8 (const char *utf8, size_t n,
                                   mulib_shylock_buffer_t *out);
#ifdef __cplusplus
}
#endif
#endif /* MULIB_WEBIDL_RUNTIME_H */
```

</details>
<!-- /manual-interface -->

<!-- manual-reference -->
## Source and build reference

[Manual index](index.md) · [Integration guide](integration.md)

| Resource | Location |
| --- | --- |
| Header | [`include/mulib-webidl-runtime.h`](../include/mulib-webidl-runtime.h) |
| Implementation switch | `MULIB_IMPL_WEBIDL_RUNTIME` |
| Repository-local includes | [mulib-shylock-core](mulib-shylock-core.md), [mulib-utf8](mulib-utf8.md) |
| Complete example | [`examples/modules/webidl-runtime.c`](../examples/modules/webidl-runtime.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/webidl-runtime.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
