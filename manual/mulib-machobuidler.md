# Mach-O builder

[Manuals](README.md) · [Library index](index.md)

Historical filename/switch retained: `MULIB_IMPL_MACHOBUIDLER`; API is
`mulib_machobuilder_text`. Also define `BYCUTILS` and `BUFFER`. Atomically appends
a deterministic x86-64 MH_OBJECT with one __TEXT,__text section, not an executable.
No symbols/relocations/signing. [Example](../examples/modules/machobuidler.c),
[contracts](storage-server.md#idl-and-native-output).

The correctly spelled [mulib-machobuilder.h](mulib-machobuilder.md) adds owned
multi-section objects and also enables this convenience API with its canonical
switch. Both switches can coexist without duplicate definitions.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-machobuidler.h`](../include/mulib-machobuidler.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-machobuidler.h
 * @brief Deterministic x86-64 MH_OBJECT with one __TEXT,__text section.
 * Historical filename retained. Define MULIB_IMPL_MACHOBUIDLER, BYCUTILS and
 * BUFFER. No symbols/relocations/signatures; not a macOS executable.
 * Atomic append, 1 success/0 invalid, overflow or allocation failure.
 * The correctly spelled machobuilder header/switch also enables this API. */
#ifndef MULIB_MACHOBUIDLER_H
#define MULIB_MACHOBUIDLER_H
#include "mulib-buffer.h"
#include "mulib-bycutils.h"
#ifdef __cplusplus
extern "C"
{
#endif
  int mulib_machobuilder_text (mulib_buffer_t *out, const void *code,
                               size_t length);
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
| Header | [`include/mulib-machobuidler.h`](../include/mulib-machobuidler.h) |
| Implementation switch | `MULIB_IMPL_MACHOBUIDLER` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-bycutils](mulib-bycutils.md) |
| Complete example | [`examples/modules/machobuidler.c`](../examples/modules/machobuidler.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/machobuidler.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
