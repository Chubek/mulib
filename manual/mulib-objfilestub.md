# objfilestub

[Manuals](README.md) · [Library index](index.md)

`x64_jump` appends 14 bytes: `FF 25 00 00 00 00` then an LE64 destination, an
indirect RIP-relative absolute jump thunk for x86 long mode. Returns 1 success,
0 invalid/allocation, output unchanged on failure. Caller owns/free buffer.

Only emits data; no executable allocation, execution, process patching, cache
synchronization or ABI/calling-convention guarantee. Not portable machine code.
Switches: `MULIB_IMPL_OBJFILESTUB`, `MULIB_IMPL_BUFFER`, `MULIB_IMPL_BYCUTILS`.
Example: `examples/modules/objfilestub.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfilestub.h`](../include/mulib-objfilestub.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfilestub.h
 * @brief Emit an x86-64 absolute indirect-jump thunk as data, never execute
 * it. Define MULIB_IMPL_OBJFILESTUB plus BUFFER and BYCUTILS. */
#ifndef MULIB_OBJFILESTUB_H
#define MULIB_OBJFILESTUB_H
#include "mulib-buffer.h"
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Append FF 25 00 00 00 00 followed by LE64 destination (14 bytes).
   * Requires long mode when executed by caller. 1 success, 0 failure
   * unchanged. */
  int mulib_objfilestub_x64_jump (mulib_buffer_t *out, uint64_t destination);
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
| Header | [`include/mulib-objfilestub.h`](../include/mulib-objfilestub.h) |
| Implementation switch | `MULIB_IMPL_OBJFILESTUB` |
| Repository-local includes | [mulib-buffer](mulib-buffer.md), [mulib-bycutils](mulib-bycutils.md) |
| Complete example | [`examples/modules/objfilestub.c`](../examples/modules/objfilestub.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfilestub.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
