# objfilereloc

[Manuals](README.md) · [Library index](index.md)

`apply` patches one bounded little-endian ABS32, ABS64 or PC32 field. Absolute
formula S+A; relative formula S+A-P, with caller-resolved symbol/addend/place.
Unsigned addresses and signed addends are overflow-checked; ABS32/PC32 ranges
checked before writes. Returns 1 success, 0 invalid/overflow/range, without mutation.

Kind constants are MuLIB kinds, **not native ELF relocation numbers**. No batch
overlap resolution, symbol lookup, architecture decoder, GOT/PLT or file I/O.
Switches: `MULIB_IMPL_OBJFILERELOC`, `MULIB_IMPL_OBJFILEPOSINDEP`,
`MULIB_IMPL_BYCUTILS`. Example: `examples/modules/objfilereloc.c`.

<!-- manual-interface -->
## Public header reference

Verbatim declaration section from [`include/mulib-objfilereloc.h`](../include/mulib-objfilereloc.h).
Include the header in application code; dependency types come from its includes.

<details>
<summary>Types, constants, and declarations</summary>

```c
/** @file mulib-objfilereloc.h
 * @brief Checked little-endian ABS32/ABS64/PC32 relocation of byte spans.
 * Define MULIB_IMPL_OBJFILERELOC plus OBJFILEPOSINDEP and BYCUTILS. */
#ifndef MULIB_OBJFILERELOC_H
#define MULIB_OBJFILERELOC_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif
  enum
  {
    MULIB_OBJFILERELOC_ABS32 = 1,
    MULIB_OBJFILERELOC_ABS64 = 2,
    MULIB_OBJFILERELOC_PC32 = 3
  };
  /** @brief Apply S+A (absolute) or S+A-P (relative) at offset; 1 success.
   * 0 invalid/overflow/range, without mutation. Caller resolves symbols. */
  int mulib_objfilereloc_apply (void *data, size_t size, size_t offset,
                                int kind, uint64_t symbol, int64_t addend,
                                uint64_t place);
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
| Header | [`include/mulib-objfilereloc.h`](../include/mulib-objfilereloc.h) |
| Implementation switch | `MULIB_IMPL_OBJFILERELOC` |
| Repository-local includes | [mulib-bycutils](mulib-bycutils.md), [mulib-objfileposindep](mulib-objfileposindep.md) |
| Complete example | [`examples/modules/objfilereloc.c`](../examples/modules/objfilereloc.c) |

The include list includes conditional and implementation-only includes;
it is not a minimal link recipe. Use the example (and any setup header it
includes) for a complete switch configuration. See the integration guide
for dependency placement and platform-specific linking.

Compile the example from the repository root on Linux:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -Iinclude examples/modules/objfilereloc.c -pthread -lm -ldl -o mulib-example
```
<!-- /manual-reference -->
