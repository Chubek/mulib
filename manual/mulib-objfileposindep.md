# objfileposindep

[Manuals](README.md) · [Library index](index.md)

`rel32` computes signed 32-bit S+A-P using checked unsigned address arithmetic.
Symbol/place are uint64, addend int64. S+A must remain a uint64 address; final
displacement must fit [-2^31,2^31-1]. P is relocation **field address**, not the
end of an instruction. Returns 1 success, 0 invalid/overflow; output unchanged.

This arithmetic primitive does not rewrite instructions, construct GOT/PLT,
parse relocation tables or certify a whole object position-independent.
Switch: `MULIB_IMPL_OBJFILEPOSINDEP`. Example: `examples/modules/objfileposindep.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfileposindep.h`](../include/mulib-objfileposindep.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfileposindep.h
 * @brief Overflow-checked PC-relative signed 32-bit displacement arithmetic.
 * Define MULIB_IMPL_OBJFILEPOSINDEP once. P denotes relocation field address.
 */
#ifndef MULIB_OBJFILEPOSINDEP_H
#define MULIB_OBJFILEPOSINDEP_H
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  /** @brief Compute S+A-P without signed overflow; 1 success, unchanged on
   * error. S+A must be representable as an unsigned 64-bit address. */
  int mulib_objfileposindep_rel32 (uint64_t symbol, int64_t addend,
                                   uint64_t place, int32_t *out);
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
| Header | [`include/mulib-objfileposindep.h`](../include/mulib-objfileposindep.h) |
| Implementation switch | `MULIB_IMPL_OBJFILEPOSINDEP` |
| Repository-local includes | None |
| Complete example | [`examples/modules/objfileposindep.c`](../examples/modules/objfileposindep.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfileposindep.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
